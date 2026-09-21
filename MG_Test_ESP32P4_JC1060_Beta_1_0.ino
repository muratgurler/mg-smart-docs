#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

#include "CableMap.h"
#include "CableMapStore.h"
#include "CableProfile.h"
#include "FrontPanelController.h"
#include "P4LvglPort.h"
#include "ReportFormatter.h"
#include "ScanScreen.h"
#include "ScanSession.h"
#include "ScanSource.h"
#include "TouchTestScreen.h"
#include "TouchControlPanel.h"
#include "MainMenuScreen.h"
#include "SettingsScreen.h"
#include "LanguageScreen.h"
#include "P4Localization.h"
#include "ProjectVersion.h"
#include "MgNetworkManager.h"
#include "NetworkStatusScreen.h"
#include "V33HardwareManager.h"
#include "AdvancedAnalysisScreen.h"
#include "ExtraFeaturesScreen.h"
#include "MapEditorScreen.h"
#include "MapGraphScreen.h"
#include "MapWebEditor.h"
#include "DocumentImportScreen.h"
#include "DocumentReviewScreen.h"
#include "DocumentImportValidator.h"
#include "MgBleProfileServer.h"
#include "DocumentAnalysisScreen.h"
#include "FeatureBackbone.h"
#include "BackboneDemoScreen.h"
#include "TestWorkflowController.h"
#include "WorkflowOverviewScreen.h"
#include "WorkflowArchiveCore.h"
#include "WorkflowArchiveScreen.h"
#include "WorkflowArchivePersistence.h"
#include "CompletionReportScreen.h"
#include "AnalysisReportScreen.h"
#include "ConfigP4.h"
#include "WorkplaceProfileHttpClient.h"
#include "WorkplaceProfileJsonAdapter.h"
#include "ConfigurableWorkplaceJsonAdapter.h"
#include "WorkplaceServerConfig.h"
#include "WorkplaceServerConfigStore.h"
#include "WorkplaceServerSettingsScreen.h"

using namespace mg::p4;

P4LvglPort displayPort;
CableMap customCableMap = makeDefaultCustomCableMap();
CableMapStore customMapStore;
DemoScanSource demoScanSource;
RoutedScanSource scanSource(demoScanSource);
ScanSession scanSession(defaultProfile(), scanSource);
ScanScreen scanScreen(scanSession);
FrontPanelController controls(scanSession, scanScreen);
TestWorkflowController testWorkflowController;
TouchTestScreen touchTestScreen;
TouchControlPanel touchControls(controls, scanSession, testWorkflowController);
MainMenuScreen mainMenuScreen;
SettingsScreen settingsScreen;
LanguageScreen languageScreen;
Preferences settings;
MgNetworkManager networkManager;
ConfigurableWorkplaceJsonAdapter workplaceResponseAdapter;
WorkplaceProfileHttpClient workplaceProfileClient(networkManager, workplaceResponseAdapter);
WorkplaceServerConfig workplaceServerConfig = defaultWorkplaceServerConfig();
WorkplaceServerConfigStore workplaceServerConfigStore;
WorkplaceServerSettingsScreen workplaceServerSettingsScreen;
NetworkStatusScreen networkStatusScreen;
mg::p4::v33::V33HardwareManager v33Hardware;
AdvancedAnalysisScreen advancedAnalysisScreen;
ExtraFeaturesScreen extraFeaturesScreen;
MapEditorScreen mapEditorScreen;
MapGraphScreen mapGraphScreen;
MapWebEditor mapWebEditor;
DocumentImportScreen documentImportScreen;
DocumentReviewScreen documentReviewScreen;
DocumentImportValidator documentImportValidator;
MgBleProfileServer bleProfileServer;
DocumentAnalysisScreen documentAnalysisScreen;
FeatureBackbone featureBackbone;
BackboneDemoScreen backboneDemoScreen(featureBackbone, &testWorkflowController);
WorkflowOverviewScreen workflowOverviewScreen;
WorkflowArchiveCore workflowArchive;
WorkflowArchivePersistence workflowArchivePersistence;
WorkflowArchiveScreen workflowArchiveScreen;
CompletionReportScreen completionReportScreen;
AnalysisReportScreen analysisReportScreen;

namespace {

void logBootHeap(const char* tag) {
    const size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t internalLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t dmaFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    const size_t dmaLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    const size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    Serial.printf("[MEM][%s] internal=%u largest=%u dma=%u dmaLargest=%u psram=%u\n",
                  tag,
                  static_cast<unsigned>(internalFree),
                  static_cast<unsigned>(internalLargest),
                  static_cast<unsigned>(dmaFree),
                  static_cast<unsigned>(dmaLargest),
                  static_cast<unsigned>(psramFree));
    Serial.flush();
}

bool reportPrinted = false;
bool completionDisplayHoldActive = false;
uint32_t completionDisplayHoldStartedMs = 0U;
bool touchTestActive = false;
bool touchTestReturnsToMenu = false;
bool touchTestReturnsToSettings = false;
bool mapEditRequested = false;
bool documentMapReviewActive = false;
enum class ActiveScreen : uint8_t {
    MainMenu,
    MapEditor,
    MapGraph,
    Settings,
    Wifi,
    Ethernet,
    Scan,
    TouchTest,
    Language,
    AdvancedAnalysis,
    ExtraFeatures,
    DocumentImport,
    DocumentReview,
    DocumentAnalysis,
    BackboneDemo,
    WorkflowOverview,
    WorkflowArchive,
    CompletionReport,
    AnalysisReport,
    WorkplaceServerSettings
};

enum class BackboneReturn : uint8_t {
    MainMenu,
    AdvancedAnalysis,
    ExtraFeatures,
    Settings
};
BackboneReturn backboneReturn = BackboneReturn::MainMenu;
ActiveScreen activeScreen = ActiveScreen::MainMenu;

void syncDemoSpecialConnections() {
    const CableProfile& profile = scanSession.profile();
    demoScanSource.setSpecialCrossConnections(
        profile.includePeA && profile.includePeB,
        profile.includeDrainShieldA && profile.includeDrainShieldB);
}

void handleLargeDirectionArrow(void*) {
    controls.handle(ControlAction::Direction);
    scanScreen.update(true);
    touchControls.refresh();
    Serial.println("[DIRECTION] Large arrow pressed; scan reversed at current contact");
    Serial.flush();
}

void handleProfileControl(ScanScreen::ProfileCommand command, void*) {
    switch (command) {
        case ScanScreen::ProfileCommand::NextSubD:
            controls.handle(ControlAction::SubDProfile);
            break;
        case ScanScreen::ProfileCommand::DecreasePins:
            controls.handle(ControlAction::NormalPinsDecrease);
            break;
        case ScanScreen::ProfileCommand::IncreasePins:
            controls.handle(ControlAction::NormalPinsIncrease);
            break;
        case ScanScreen::ProfileCommand::NextPinPreset:
            controls.handle(ControlAction::NormalPinsNextPreset);
            break;
        case ScanScreen::ProfileCommand::TogglePeA:
            controls.handle(ControlAction::TogglePeA);
            break;
        case ScanScreen::ProfileCommand::ToggleDrainShieldA:
            controls.handle(ControlAction::ToggleDrainShieldA);
            break;
        case ScanScreen::ProfileCommand::TogglePeB:
            controls.handle(ControlAction::TogglePeB);
            break;
        case ScanScreen::ProfileCommand::ToggleDrainShieldB:
            controls.handle(ControlAction::ToggleDrainShieldB);
            break;
        case ScanScreen::ProfileCommand::EditMap:
            // Defer the screen change until the main loop so the LVGL button
            // callback can finish cleanly.
            scanSession.pause();
            mapEditRequested = true;
            break;
    }
    syncDemoSpecialConnections();
    scanScreen.update(true);
    touchControls.refresh();
}

void showMainMenu() {
    scanSession.pause();
    displayPort.setBacklight(true);
    mainMenuScreen.begin();
    activeScreen = ActiveScreen::MainMenu;
    displayPort.update();
}

void showSettings() {
    scanSession.pause();
    displayPort.setBacklight(true);
    settingsScreen.begin();
    activeScreen = ActiveScreen::Settings;
    displayPort.update();
}

void applyWorkplaceServerConfig() {
    WorkplaceHttpConfig http{};
    http.prEndpointTemplate = workplaceServerConfig.prEndpointTemplate;
    http.tlsCaPem = workplaceServerConfig.tlsCaPem;
    http.timeoutMs = workplaceServerConfig.timeoutMs;
    http.maxResponseBytes = workplaceServerConfig.maxResponseBytes;
    http.authMode = workplaceServerConfig.authMode;
    http.authUser = workplaceServerConfig.authUser;
    http.authSecret = workplaceServerConfig.authSecret;
    http.customHeaderName = workplaceServerConfig.customHeaderName;
    http.responseConfigurationValid = validateWorkplaceServerConfig(workplaceServerConfig).responseValid;
    workplaceResponseAdapter.configure(workplaceServerConfig.responseFormat, workplaceServerConfig.fieldMap);
    workplaceProfileClient.configure(http);
}

void showWorkplaceServerSettings() {
    scanSession.pause();
    displayPort.setBacklight(true);
    workplaceServerSettingsScreen.begin(workplaceServerConfig, workplaceServerConfigStore.statusText());
    activeScreen = ActiveScreen::WorkplaceServerSettings;
    displayPort.update();
    Serial.println("[WORKPLACE-CONFIG] settings opened; secrets are not printed");
    Serial.flush();
}


void showAdvancedAnalysis() {
    scanSession.pause();
    displayPort.setBacklight(true);
    advancedAnalysisScreen.begin(v33Hardware);
    activeScreen = ActiveScreen::AdvancedAnalysis;
    displayPort.update();
}

void showExtraFeatures() {
    scanSession.pause();
    displayPort.setBacklight(true);
    extraFeaturesScreen.begin();
    activeScreen = ActiveScreen::ExtraFeatures;
    displayPort.update();
}

void showWorkflowOverview() {
    scanSession.pause();
    displayPort.setBacklight(true);
    workflowOverviewScreen.begin(testWorkflowController);
    activeScreen = ActiveScreen::WorkflowOverview;
    displayPort.update();
    Serial.println("[WORKFLOW] overview opened");
    Serial.flush();
}

void showWorkflowArchive() {
    scanSession.pause();
    displayPort.setBacklight(true);
    workflowArchiveScreen.begin(workflowArchive);
    workflowArchiveScreen.setStorageStatus(workflowArchivePersistence.statusText());
    activeScreen = ActiveScreen::WorkflowArchive;
    displayPort.update();
    Serial.printf("[ARCHIVE] records=%u cache=%u\n",
                  static_cast<unsigned>(workflowArchive.resultCount()),
                  static_cast<unsigned>(workflowArchive.profileCacheCount()));
    Serial.flush();
}

void showTestCompletionReport() {
    scanSession.pause();
    displayPort.setBacklight(true);
    (void)workflowArchivePersistence.saveNow();
    completionReportScreen.beginTest(testWorkflowController, scanSession);
    activeScreen = ActiveScreen::CompletionReport;
    displayPort.update();
    Serial.printf("[REPORT-UI] finite test complete verdict=%s errors=%u; report opened\n",
                  TestWorkflowController::verdictText(testWorkflowController.snapshot().verdict),
                  static_cast<unsigned>(testWorkflowController.snapshot().electricalErrors));
    Serial.flush();
}

void showCableLearnCompletionReport() {
    displayPort.setBacklight(true);
    completionReportScreen.beginCableLearn(backboneDemoScreen.cableLearnProfile(),
                                           backboneDemoScreen.cableLearnMap(),
                                           backboneDemoScreen.cableLearnNetCount(),
                                           backboneDemoScreen.cableLearnDemoMode());
    activeScreen = ActiveScreen::CompletionReport;
    displayPort.update();
    Serial.printf("[LEARN-REPORT] complete nets=%u; operator review/save required\n",
                  static_cast<unsigned>(backboneDemoScreen.cableLearnNetCount()));
    Serial.flush();
}

void showAnalysisCompletionReport() {
    displayPort.setBacklight(true);
    analysisReportScreen.begin(backboneDemoScreen.analysisReport());
    activeScreen = ActiveScreen::AnalysisReport;
    displayPort.update();
    Serial.printf("[ANALYSIS-REPORT] module=%u rows=%u live=%u; table opened\n",
                  static_cast<unsigned>(backboneDemoScreen.analysisReport().module),
                  static_cast<unsigned>(backboneDemoScreen.analysisReport().rowCount),
                  backboneDemoScreen.analysisReport().liveSnapshot ? 1U : 0U);
    Serial.flush();
}

void showBackboneDemo(BackboneModuleId module,
                      const char* workflowTitle,
                      BackboneReturn returnTo) {
    scanSession.pause();
    displayPort.setBacklight(true);
    backboneReturn = returnTo;
    backboneDemoScreen.begin(module, workflowTitle);
    activeScreen = ActiveScreen::BackboneDemo;
    displayPort.update();
    Serial.printf("[BACKBONE] demo screen module=%u return=%u\n",
                  static_cast<unsigned>(module),
                  static_cast<unsigned>(returnTo));
    Serial.flush();
}

void returnFromBackboneDemo() {
    switch (backboneReturn) {
        case BackboneReturn::MainMenu:
            showMainMenu();
            break;
        case BackboneReturn::AdvancedAnalysis:
            showAdvancedAnalysis();
            break;
        case BackboneReturn::ExtraFeatures:
            showExtraFeatures();
            break;
        case BackboneReturn::Settings:
            showSettings();
            break;
    }
}

void showDocumentImport() {
    scanSession.pause();
    displayPort.setBacklight(true);
    documentImportScreen.begin(mapWebEditor, networkManager, bleProfileServer);
    activeScreen = ActiveScreen::DocumentImport;
    displayPort.update();
    Serial.println("[DOC-IMPORT] P4 document transfer screen opened");
    Serial.flush();
}


void showDocumentReview() {
    scanSession.pause();
    bleProfileServer.closeSession();
    displayPort.setBacklight(true);
    if (mapWebEditor.documentItemCount() == 0U) {
        (void)mapWebEditor.recoverDocumentItemsFromStorage();
    }
    documentReviewScreen.begin(mapWebEditor);
    activeScreen = ActiveScreen::DocumentReview;
    displayPort.update();
    Serial.println("[DOC-REVIEW] document set review opened");
    Serial.flush();
}

void showDocumentAnalysis() {
    scanSession.pause();
    bleProfileServer.closeSession();
    displayPort.setBacklight(true);
    documentAnalysisScreen.begin(mapWebEditor, documentImportValidator);
    activeScreen = ActiveScreen::DocumentAnalysis;
    displayPort.update();
    Serial.println("[MOBILE-PROFILE] validation screen opened");
    Serial.flush();
}

void showMapEditor() {
    scanSession.pause();
    displayPort.setBacklight(true);
    documentMapReviewActive = false;
    mapWebEditor.consumeChanged();
    mapEditorScreen.begin(customCableMap, controls.normalProfile());
    activeScreen = ActiveScreen::MapEditor;
    displayPort.update();
    Serial.println("[MAP-EDITOR] 7-inch touch editor opened");
    Serial.flush();
}

void showDocumentMapReview() {
    scanSession.pause();
    displayPort.setBacklight(true);
    documentMapReviewActive = true;
    mapEditorScreen.begin(documentAnalysisScreen.draftMap(),
                          documentAnalysisScreen.draftProfile(),
                          MapEditorContext::DocumentReview);
    activeScreen = ActiveScreen::MapEditor;
    displayPort.update();
    Serial.println("[MOBILE-PROFILE] full map review opened");
    Serial.flush();
}

void showMapGraph() {
    scanSession.pause();
    displayPort.setBacklight(true);
    // Preview the editor's WORKING copy, not only the last saved map.
    const CableProfile& graphProfile = documentMapReviewActive
        ? documentAnalysisScreen.draftProfile()
        : controls.normalProfile();
    mapGraphScreen.begin(mapEditorScreen.workingMap(), graphProfile);
    activeScreen = ActiveScreen::MapGraph;
    displayPort.update();
    Serial.printf("[MAP-GRAPH] pins=%u A(PE=%u,dS=%u) B(PE=%u,dS=%u)\n",
                  static_cast<unsigned>(graphProfile.signalPinCount),
                  graphProfile.includePeA ? 1U : 0U,
                  graphProfile.includeDrainShieldA ? 1U : 0U,
                  graphProfile.includePeB ? 1U : 0U,
                  graphProfile.includeDrainShieldB ? 1U : 0U);
    Serial.flush();
}

void showNetworkScreen(NetworkScreenMode mode) {
    scanSession.pause();
    displayPort.setBacklight(true);
    networkStatusScreen.begin(mode, networkManager);
    activeScreen = mode == NetworkScreenMode::Wifi ? ActiveScreen::Wifi : ActiveScreen::Ethernet;
    displayPort.update();
}

void startScanUi(ScanProfileKind kind, bool customMapMode = false, bool workflowPrepared = false) {
    reportPrinted = false;
    completionDisplayHoldActive = false;
    completionDisplayHoldStartedMs = 0U;
    Serial.println("[BOOT][02] building scan screen");
    Serial.flush();
    if (workflowPrepared && testWorkflowController.hasPreparedProfile()) {
        controls.loadPreparedProfile(testWorkflowController.profile());
        kind = testWorkflowController.profile().kind;
    } else {
        controls.selectScanMode(kind);
    }
    syncDemoSpecialConnections();

    // Keep expected-map analysis and the demo physical cable synchronized.
    // RoutedScanSource keeps DEMO active while hardware is gated. Once the
    // carrier is validated, only the physical observation backend changes;
    // ScanSession custom-map classification remains unchanged.
    if (customMapMode) {
        if (workflowPrepared && testWorkflowController.usesCustomMap()) {
            customCableMap = testWorkflowController.map();
        }
        scanSession.setCustomMap(customCableMap);
        demoScanSource.setPhysicalMap(&customCableMap);
    } else {
        scanSession.setOneToOneMap();
        demoScanSource.setPhysicalMap(nullptr);
    }

    if (!workflowPrepared) {
        testWorkflowController.prepareManual(scanSession.profile(),
                                             customMapMode,
                                             customMapMode ? &customCableMap : nullptr);
        Serial.printf("[WORKFLOW] manual profile READY source=%u map=%s autoStart=0\n",
                      static_cast<unsigned>(testWorkflowController.snapshot().source),
                      customMapMode ? "NET" : "1:1");
        Serial.flush();
    }

    if (scanScreen.root() == nullptr) {
        scanScreen.setDirectionHandler(handleLargeDirectionArrow, nullptr);
        scanScreen.setProfileHandler(handleProfileControl, nullptr);
        scanScreen.begin();
        touchControls.begin(scanScreen.root());
    } else {
        scanScreen.activate();
        scanScreen.onProfileChanged();
    }
    Serial.println("[BOOT][03] scan screen and touch controls ready");
    Serial.flush();
    displayPort.update();
    scanSession.reset();
    activeScreen = ActiveScreen::Scan;
    touchControls.refresh();
    Serial.printf("[BOOT] P4 %s / %s scan UI ready; waiting for START.\n",
                  kind == ScanProfileKind::SubD ? "SUB-D" : "NORMAL",
                  customMapMode ? customCableMap.name() : "ONE_TO_ONE");
    Serial.flush();
}

void startTouchTest() {
    if (touchTestActive) {
        return;
    }
    touchTestReturnsToMenu = activeScreen == ActiveScreen::MainMenu;
    touchTestReturnsToSettings = activeScreen == ActiveScreen::Settings;
    scanSession.pause();
    touchTestActive = true;
    activeScreen = ActiveScreen::TouchTest;
    displayPort.setBacklight(true);
    touchTestScreen.begin();
    displayPort.update();
    Serial.println("[DIAGNOSTIC] Five-point touch test opened");
    Serial.flush();
}

void returnToScanUi() {
    if (!touchTestActive) {
        return;
    }
    touchTestActive = false;
    if (touchTestReturnsToSettings) {
        touchTestReturnsToSettings = false;
        settingsScreen.begin();
        activeScreen = ActiveScreen::Settings;
        displayPort.update();
        Serial.println("[DIAGNOSTIC] Returned to settings");
        Serial.flush();
        return;
    }
    if (touchTestReturnsToMenu) {
        touchTestReturnsToMenu = false;
        showMainMenu();
        Serial.println("[DIAGNOSTIC] Returned to main menu");
        Serial.flush();
        return;
    }
    scanScreen.activate();
    activeScreen = ActiveScreen::Scan;
    scanScreen.update();
    touchControls.refresh();
    displayPort.update();
    Serial.println("[DIAGNOSTIC] Returned to scan UI; scan remains paused");
    Serial.flush();
}

void openLanguageScreen() {
    scanSession.pause();
    languageScreen.begin();
    activeScreen = ActiveScreen::Language;
    displayPort.update();
}

void saveLanguage(P4Language language) {
    setP4Language(language);
    settings.begin("mg-test", false);
    settings.putUChar("language", static_cast<uint8_t>(language));
    settings.end();
    Serial.printf("[LANGUAGE] selected=%s\n", p4LanguageName(language));
    Serial.flush();
}

void loadLanguage() {
    settings.begin("mg-test", true);
    const uint8_t saved = settings.getUChar(
        "language", static_cast<uint8_t>(P4Language::Turkish));
    settings.end();
    setP4Language(isValidP4Language(saved)
                      ? static_cast<P4Language>(saved)
                      : P4Language::Turkish);
}

void printDirectionReport(ScanDirection direction) {
    Serial.println(direction == ScanDirection::AtoB
                       ? "[REPORT] A_TO_B"
                       : "[REPORT] B_TO_A");
    Serial.println("DIRECTION,POINT,EXPECTED,ACTUAL,RESULT");
    char row[96] = {};
    for (uint8_t slot = 0;
         slot < scanSession.profile().activePointCount();
         ++slot) {
        if (formatReportRow(scanSession,
                            direction,
                            slot,
                            row,
                            sizeof(row))) {
            Serial.println(row);
        }
    }
}

void handleWorkflowRetestRequest(const char* origin) {
    if (!testWorkflowController.prepareRetest()) {
        Serial.printf("[WORKFLOW] RETEST rejected origin=%s state=%u\n",
                      origin != nullptr ? origin : "?",
                      static_cast<unsigned>(testWorkflowController.snapshot().state));
        Serial.flush();
        return;
    }
    (void)workflowArchivePersistence.saveNow();
    if (testWorkflowController.usesCustomMap()) customCableMap = testWorkflowController.map();
    startScanUi(testWorkflowController.profile().kind,
                testWorkflowController.usesCustomMap(), true);
    Serial.printf("[WORKFLOW] RETEST origin=%s generation=%lu attempt=%u READY; waiting START\n",
                  origin != nullptr ? origin : "?",
                  static_cast<unsigned long>(testWorkflowController.snapshot().generation),
                  static_cast<unsigned>(testWorkflowController.snapshot().attempt));
    Serial.flush();
}

void handleWorkflowNextCableRequest(const char* origin) {
    (void)workflowArchivePersistence.saveNow();
    const NextCableDisposition disposition = testWorkflowController.prepareNextCable();
    if (disposition == NextCableDisposition::ReadySameProfile) {
        if (testWorkflowController.usesCustomMap()) customCableMap = testWorkflowController.map();
        startScanUi(testWorkflowController.profile().kind,
                    testWorkflowController.usesCustomMap(), true);
        Serial.printf("[WORKFLOW] NEXT CABLE origin=%s same profile READY; waiting START\n",
                      origin != nullptr ? origin : "?");
        Serial.flush();
    } else if (disposition == NextCableDisposition::NeedsNewIdentifier) {
        showBackboneDemo(BackboneModuleId::BarcodeQr,
                         currentP4Language() == P4Language::Turkish ? "SONRAKI KABLO / KOD OKU" : "NEXT CABLE / SCAN CODE",
                         BackboneReturn::MainMenu);
        Serial.printf("[WORKFLOW] NEXT CABLE origin=%s requires new PR/customer identifier\n",
                      origin != nullptr ? origin : "?");
        Serial.flush();
    } else {
        Serial.printf("[WORKFLOW] NEXT CABLE rejected origin=%s state=%u\n",
                      origin != nullptr ? origin : "?",
                      static_cast<unsigned>(testWorkflowController.snapshot().state));
        Serial.flush();
    }
}

void handleSerialControl(char command) {
    switch (command) {
        case 's':
        case 'S':
            controls.handle(ControlAction::AutoStartPause);
            break;
        case 'n':
        case 'N':
            controls.handle(ControlAction::Step);
            break;
        case 'd':
        case 'D':
            controls.handle(ControlAction::Direction);
            break;
        case 'r':
        case 'R':
            controls.handle(ControlAction::ResetAbort);
            break;
        case 'p':
        case 'P':
            controls.handle(ControlAction::SubDProfile);
            break;
        case 'e':
        case 'E':
            controls.handle(ControlAction::StopOnError);
            break;
        case 'c':
        case 'C':
            controls.handle(ControlAction::ContinuousMode);
            break;
        case 't':
        case 'T':
            startTouchTest();
            break;
        case 'x':
        case 'X':
            controls.handle(ControlAction::NextCable);
            break;
        case 'y':
        case 'Y':
            controls.handle(ControlAction::Retest);
            break;
        default:
            break;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("%s | %s | BLE-FIRST MOBILE PROFILE TRANSFER\n",
                  mg::p4::build::kProductName,
                  mg::p4::build::kReleaseName);
    Serial.println("BOARD=JC1060P470C_I_W LCD=JD9165 1024x600 TOUCH=GT911");
    Serial.println("[PROFILE] On-device recognition removed; mobile prepared-profile import enabled");
    Serial.println("S=start/pause N=step D=direction R=reset P=profile E=stop C=non-stop T=touch X=next-cable Y=retest");
    Serial.flush();

    // Start the C6 ESP-Hosted transport before the display stack. This proven
    // ordering protects the scarce internal/DMA heap on JC1060.
    logBootHeap("PRE-HOSTED");
    Serial.println("[BOOT][00] ESP-Hosted/Wi-Fi transport initialization");
    Serial.flush();
    networkManager.begin();
    logBootHeap("POST-HOSTED");

    // BLE-first phone transfer uses the onboard ESP32-C6 as the remote
    // controller over the already-started ESP-Hosted transport. Initialize it
    // before LCD/LVGL so its internal/DMA allocations happen in the proven
    // boot window. Wi-Fi fallback remains operational if BLE init fails.
    Serial.println("[BOOT][00B] ESP-Hosted/NimBLE profile receiver initialization");
    Serial.flush();
    const bool bleReady = bleProfileServer.begin(mapWebEditor, documentImportValidator);
    Serial.printf("[BOOT][00B] BLE profile receiver=%s\n", bleReady ? "READY" : "FALLBACK-WIFI");
    Serial.flush();
    logBootHeap("POST-BLE");

    Serial.println("[BOOT][01] display/touch initialization");
    Serial.flush();
    if (!displayPort.begin()) {
        Serial.println("[BOOT] Display/touch initialization failed.");
        while (true) {
            delay(1000);
        }
    }
    logBootHeap("POST-DISPLAY");

    loadLanguage();
    if (customMapStore.load(customCableMap)) {
        Serial.println("[MAP] saved custom map loaded from Preferences");
    } else {
        Serial.println("[MAP] no saved custom map; built-in first-use map active");
    }
    Serial.flush();
    workplaceServerConfig = defaultWorkplaceServerConfig();
    (void)workplaceServerConfigStore.load(workplaceServerConfig);
    applyWorkplaceServerConfig();
    const WorkplaceServerConfigValidation workplaceValidation =
        validateWorkplaceServerConfig(workplaceServerConfig);
    Serial.printf("[WORKPLACE] PR HTTP=%s auth=%s response=%s saved=%u autoStart=0\n",
                  workplaceValidation.ready ? "CONFIGURED" : "CONFIG_REQUIRED",
                  workplaceAuthModeName(workplaceServerConfig.authMode),
                  workplaceResponseFormatName(workplaceServerConfig.responseFormat),
                  workplaceServerConfigStore.hasSavedConfig() ? 1U : 0U);
    Serial.flush();
    mapWebEditor.begin(customCableMap, customMapStore, networkManager);
    v33Hardware.begin();
    scanSource.attachPhysical(v33Hardware.digitalScanSource());
    scanSource.preferPhysical(v33Hardware.hardwareEnabled() &&
                              v33Hardware.digital().state == mg::p4::v33::HardwareState::Ready);
    Serial.printf("[SCAN-SOURCE] active=%s physicalAttached=%u hardwareEnabled=%u\n",
                  scanSource.isDemo() ? "DEMO" : "MCP23S17",
                  scanSource.physicalAttached() ? 1U : 0U,
                  v33Hardware.hardwareEnabled() ? 1U : 0U);
    Serial.flush();
    featureBackbone.begin(v33Hardware);
    workflowArchive.reset();
    workflowArchivePersistence.begin(workflowArchive);
    (void)workflowArchivePersistence.load();
    testWorkflowController.attachArchive(workflowArchive);
    Serial.printf("[ARCHIVE] persistence=%s records=%u cache=%u\n",
                  workflowArchivePersistence.statusText(),
                  static_cast<unsigned>(workflowArchive.resultCount()),
                  static_cast<unsigned>(workflowArchive.profileCacheCount()));
    Serial.flush();
    showMainMenu();
}

void loop() {
    while (Serial.available() > 0) {
        const char command = static_cast<char>(Serial.read());
        if (touchTestActive && (command == 'k' || command == 'K')) {
            Serial.println("[TOUCHTEST] Closed from serial monitor");
            returnToScanUi();
        } else if (activeScreen == ActiveScreen::Scan) {
            handleSerialControl(command);
        }
    }

    // Network and browser map editor remain responsive on every screen.
    networkManager.tick();
    bleProfileServer.tick();
    mapWebEditor.setEditingEnabled(activeScreen != ActiveScreen::Scan &&
                                   activeScreen != ActiveScreen::MapEditor &&
                                   activeScreen != ActiveScreen::MapGraph &&
                                   activeScreen != ActiveScreen::DocumentImport &&
                                   activeScreen != ActiveScreen::DocumentReview &&
                                   activeScreen != ActiveScreen::DocumentAnalysis &&
                                   activeScreen != ActiveScreen::BackboneDemo &&
                                   activeScreen != ActiveScreen::WorkflowOverview);
    mapWebEditor.tick();
    workflowArchivePersistence.tick();

    if (activeScreen == ActiveScreen::MainMenu) {
        const MainMenuAction action = mainMenuScreen.consumeAction();
        if (action == MainMenuAction::NormalScan) {
            startScanUi(ScanProfileKind::Normal);
        } else if (action == MainMenuAction::SubDScan) {
            startScanUi(ScanProfileKind::SubD);
        } else if (action == MainMenuAction::AdvancedAnalysis) {
            showAdvancedAnalysis();
        } else if (action == MainMenuAction::ExtraFeatures) {
            showExtraFeatures();
        } else if (action == MainMenuAction::Settings) {
            showSettings();
        } else if (action == MainMenuAction::CableLearn) {
            showBackboneDemo(BackboneModuleId::CableLearn,
                             currentP4Language() == P4Language::Turkish ? "KABLO OGRENME" : "CABLE LEARN",
                             BackboneReturn::MainMenu);
        } else if (action == MainMenuAction::QrBarcode) {
            showBackboneDemo(BackboneModuleId::BarcodeQr,
                             currentP4Language() == P4Language::Turkish ? "BARCODE / QR PROFIL" : "BARCODE / QR PROFILE",
                             BackboneReturn::MainMenu);
        } else if (action == MainMenuAction::Reports) {
            showWorkflowOverview();
        } else if (action == MainMenuAction::MultiConnector) {
            showBackboneDemo(BackboneModuleId::MultiConnector,
                             currentP4Language() == P4Language::Turkish ? "COKLU KONNEKTOR TESTI" : "MULTI-CONNECTOR TEST",
                             BackboneReturn::MainMenu);
        } else if (action == MainMenuAction::Language) {
            openLanguageScreen();
        }
        displayPort.update();
        delay(5);
        return;
    }


    if (activeScreen == ActiveScreen::MapEditor) {
        if (!documentMapReviewActive && !mapEditorScreen.dirty() && mapWebEditor.consumeChanged()) {
            mapEditorScreen.reloadFrom(customCableMap);
        }
        const MapEditorAction action = mapEditorScreen.consumeAction();
        if (action == MapEditorAction::Graph) {
            showMapGraph();
        } else if (documentMapReviewActive) {
            if (action == MapEditorAction::Save || action == MapEditorAction::DocumentDone) {
                const bool accepted = documentAnalysisScreen.acceptOperatorEditedMap(mapEditorScreen.workingMap());
                Serial.printf("[MOBILE-PROFILE] map-review=%s\n", accepted ? "OK" : "BLOCKED");
                Serial.flush();
                if (accepted) {
                    mapEditorScreen.markSaved();
                }
                if ((action == MapEditorAction::DocumentDone && accepted) || !accepted) {
                    // A rejected edit returns to the operator summary with an
                    // actionable message; the last valid map remains intact.
                    documentMapReviewActive = false;
                    documentAnalysisScreen.activate();
                    activeScreen = ActiveScreen::DocumentAnalysis;
                }
            } else if (action == MapEditorAction::Back) {
                documentMapReviewActive = false;
                documentAnalysisScreen.activate();
                activeScreen = ActiveScreen::DocumentAnalysis;
            }
        } else if (action == MapEditorAction::Save || action == MapEditorAction::Test) {
            customCableMap = mapEditorScreen.workingMap();
            const bool saved = customMapStore.save(customCableMap);
            Serial.printf("[MAP-EDITOR] save=%s\n", saved ? "OK" : "FAILED");
            Serial.flush();
            mapEditorScreen.markSaved();

            // Saving from NORMAL CABLE TEST immediately makes this the
            // expected map for the current normal profile. There is no
            // separate "special map" test mode anymore.
            scanSession.setCustomMap(customCableMap);
            demoScanSource.setPhysicalMap(&customCableMap);

            if (action == MapEditorAction::Test) {
                startScanUi(ScanProfileKind::Normal, true);
            }
        } else if (action == MapEditorAction::Back) {
            scanScreen.activate();
            scanScreen.onProfileChanged();
            activeScreen = ActiveScreen::Scan;
            touchControls.refresh();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::MapGraph) {
        // Service the two post-present full-frame refreshes before
        // the regular LVGL handler below. This removes the need to touch
        // EXPECTED/MEASURED/COMBINED just to make all pin numbers appear.
        mapGraphScreen.servicePostPresentRefresh();
        if (mapGraphScreen.consumeBackRequest()) {
            if (mapGraphScreen.returnsToCompletionReport()) {
                completionReportScreen.activate();
                activeScreen = ActiveScreen::CompletionReport;
            } else {
                mapEditorScreen.activate();
                activeScreen = ActiveScreen::MapEditor;
            }
            displayPort.update();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::AdvancedAnalysis) {
        v33Hardware.tick();
        advancedAnalysisScreen.update(v33Hardware);
        BackboneModuleId requestedModule = BackboneModuleId::Kelvin;
        const char* workflowTitle = nullptr;
        if (advancedAnalysisScreen.consumeConnectionScanRequest()) {
            startScanUi(ScanProfileKind::Normal);
        } else if (advancedAnalysisScreen.consumeBackboneRequest(requestedModule, workflowTitle)) {
            showBackboneDemo(requestedModule, workflowTitle, BackboneReturn::AdvancedAnalysis);
        } else if (advancedAnalysisScreen.consumeBackRequest()) {
            showMainMenu();
        }
        networkManager.tick();
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::ExtraFeatures) {
        BackboneModuleId requestedModule = BackboneModuleId::SelfTestCalibration;
        const char* workflowTitle = nullptr;
        if (extraFeaturesScreen.consumeDocumentImportRequest()) {
            showDocumentImport();
        } else if (extraFeaturesScreen.consumeBackboneRequest(requestedModule, workflowTitle)) {
            showBackboneDemo(requestedModule, workflowTitle, BackboneReturn::ExtraFeatures);
        } else if (extraFeaturesScreen.consumeBackRequest()) {
            showMainMenu();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::CompletionReport) {
        completionReportScreen.refresh();
        const CompletionReportAction action = completionReportScreen.consumeAction();
        if (action == CompletionReportAction::MainMenu) {
            showMainMenu();
        } else if (action == CompletionReportAction::Retest) {
            handleWorkflowRetestRequest("END_REPORT");
        } else if (action == CompletionReportAction::Graph) {
            if (completionReportScreen.kind() == CompletionReportKind::Test) {
                mapGraphScreen.beginResults(scanSession);
            } else {
                mapGraphScreen.begin(completionReportScreen.learnedMap(),
                                     completionReportScreen.learnedProfile(),
                                     true);
            }
            activeScreen = ActiveScreen::MapGraph;
            displayPort.update();
        } else if (action == CompletionReportAction::Records) {
            showWorkflowArchive();
        } else if (action == CompletionReportAction::ReturnToLearning) {
            backboneDemoScreen.activate();
            activeScreen = ActiveScreen::BackboneDemo;
            displayPort.update();
        } else if (action == CompletionReportAction::SaveLearnedProfile) {
            if (testWorkflowController.prepareCableLearn(completionReportScreen.learnedProfile(),
                                                         completionReportScreen.learnedMap())) {
                (void)workflowArchivePersistence.saveNow();
                completionReportScreen.markLearnedProfileSaved(true);
                Serial.println("[LEARN-REPORT] learned electrical-net profile saved to workflow/cache; READY; no autostart");
            } else {
                completionReportScreen.markLearnedProfileSaved(false);
                Serial.println("[LEARN-REPORT] save rejected by workflow controller");
            }
            Serial.flush();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::AnalysisReport) {
        analysisReportScreen.refresh();
        const AnalysisReportAction action = analysisReportScreen.consumeAction();
        if (action == AnalysisReportAction::ReturnToMeasurement) {
            backboneDemoScreen.activate();
            activeScreen = ActiveScreen::BackboneDemo;
            displayPort.update();
        } else if (action == AnalysisReportAction::ReturnToOriginMenu) {
            returnFromBackboneDemo();
        } else if (action == AnalysisReportAction::MainMenu) {
            showMainMenu();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::WorkflowOverview) {
        workflowOverviewScreen.update();
        const WorkflowOverviewAction action = workflowOverviewScreen.consumeAction();
        if (action == WorkflowOverviewAction::OpenHistory) {
            showWorkflowArchive();
        } else if (action == WorkflowOverviewAction::Retest) {
            handleWorkflowRetestRequest("REPORT_TOUCH");
        } else if (action == WorkflowOverviewAction::NextCable) {
            handleWorkflowNextCableRequest("REPORT_TOUCH");
        } else if (action == WorkflowOverviewAction::OpenSpc) {
            showBackboneDemo(BackboneModuleId::FixtureSpc,
                             currentP4Language() == P4Language::Turkish ? "RAPOR / SPC" : "REPORTS / SPC",
                             BackboneReturn::MainMenu);
        } else if (action == WorkflowOverviewAction::Back) {
            showMainMenu();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::WorkflowArchive) {
        workflowArchiveScreen.update();
        const WorkflowArchiveAction action = workflowArchiveScreen.consumeAction();
        if (action == WorkflowArchiveAction::LoadSelectedProfile) {
            TestProfileSource source = TestProfileSource::None;
            TestWorkflowIdentity identity{};
            const CableProfile* profile = nullptr;
            const CableMap* map = nullptr;
            bool customMap = false;
            if (workflowArchive.loadCachedProfile(workflowArchiveScreen.selectedProfileOffset(),
                                                  source, identity, profile, map, customMap) &&
                profile != nullptr && map != nullptr &&
                testWorkflowController.prepareCachedProfile(*profile, *map, customMap, identity)) {
                if (customMap) customCableMap = *map;
                startScanUi(profile->kind, customMap, true);
                Serial.printf("[ARCHIVE] cached profile loaded source=%u originalSource=%u READY; waiting START\n",
                              static_cast<unsigned>(testWorkflowController.snapshot().source),
                              static_cast<unsigned>(source));
                Serial.flush();
            }
        } else if (action == WorkflowArchiveAction::ExportSelectedResultCsv) {
            (void)workflowArchivePersistence.exportRecordCsv(workflowArchiveScreen.selectedResultOffset());
            workflowArchiveScreen.setStorageStatus(workflowArchivePersistence.statusText());
            Serial.printf("[REPORT] selected CSV %s path=%s\n",
                          workflowArchivePersistence.statusText(),
                          workflowArchivePersistence.lastExportPath());
            Serial.flush();
        } else if (action == WorkflowArchiveAction::ExportAllResultsCsv) {
            (void)workflowArchivePersistence.exportAllCsv();
            workflowArchiveScreen.setStorageStatus(workflowArchivePersistence.statusText());
            Serial.printf("[REPORT] all CSV %s path=%s\n",
                          workflowArchivePersistence.statusText(),
                          workflowArchivePersistence.lastExportPath());
            Serial.flush();
        } else if (action == WorkflowArchiveAction::Back) {
            showWorkflowOverview();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::BackboneDemo) {
        backboneDemoScreen.update();
        if (backboneDemoScreen.consumeCableLearnCompletion()) {
            showCableLearnCompletionReport();
            return;
        }
        if (backboneDemoScreen.consumeAnalysisReportRequest()) {
            showAnalysisCompletionReport();
            return;
        }
        if (backboneDemoScreen.consumeBackRequest()) {
            returnFromBackboneDemo();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::DocumentImport) {
        documentImportScreen.update();
        if (documentImportScreen.consumeReviewRequest()) {
            // Skip the engineering-oriented file/CRC review in
            // normal operator flow. The single profile summary performs the
            // same fail-closed validation automatically.
            showDocumentAnalysis();
        } else if (documentImportScreen.consumeBackRequest()) {
            bleProfileServer.closeSession();
            mapWebEditor.stopDocumentPortal();
            showExtraFeatures();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::DocumentReview) {
        const DocumentReviewAction action = documentReviewScreen.consumeAction();
        if (action == DocumentReviewAction::ReturnToUpload) {
            documentImportScreen.activate();
            activeScreen = ActiveScreen::DocumentImport;
        } else if (action == DocumentReviewAction::PrepareAnalysis) {
            showDocumentAnalysis();
        } else if (action == DocumentReviewAction::Back) {
            bleProfileServer.closeSession();
            mapWebEditor.stopDocumentPortal();
            showExtraFeatures();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::DocumentAnalysis) {
        const DocumentAnalysisAction action = documentAnalysisScreen.consumeAction();
        if (action == DocumentAnalysisAction::ReturnToReview) {
            documentImportScreen.activate();
            activeScreen = ActiveScreen::DocumentImport;
        } else if (action == DocumentAnalysisAction::EditDraftMap) {
            if (documentAnalysisScreen.hasEditableDraft()) showDocumentMapReview();
        } else if (action == DocumentAnalysisAction::ApplyProfile) {
            if (documentAnalysisScreen.hasDraftProfile()) {
                customCableMap = documentAnalysisScreen.draftMap();
                const ProductionProfileRecord& mobileRecord = documentAnalysisScreen.profileRecord();
                TestWorkflowIdentity mobileIdentity{};
                snprintf(mobileIdentity.productionPr, sizeof(mobileIdentity.productionPr), "%s", mobileRecord.productionPr);
                if (strcmp(mobileRecord.customerReference, "MOBILE-IMPORT") != 0) {
                    snprintf(mobileIdentity.customerReference, sizeof(mobileIdentity.customerReference), "%s", mobileRecord.customerReference);
                }
                if (strcmp(mobileRecord.revision, "UNSPECIFIED") != 0) {
                    snprintf(mobileIdentity.revision, sizeof(mobileIdentity.revision), "%s", mobileRecord.revision);
                }
                snprintf(mobileIdentity.profileId, sizeof(mobileIdentity.profileId), "%s", mobileRecord.profileId);
                snprintf(mobileIdentity.sourceCode, sizeof(mobileIdentity.sourceCode), "%s",
                         documentAnalysisScreen.operatorEdited() ? "MOBILE-BLE-EDITED" : "MOBILE-BLE");
                testWorkflowController.prepareDocument(documentAnalysisScreen.draftProfile(),
                                                       customCableMap,
                                                       mobileIdentity);
                controls.loadDocumentProfile(documentAnalysisScreen.draftProfile());
                mapWebEditor.stopDocumentPortal();
                startScanUi(documentAnalysisScreen.draftProfile().kind, true, true);
                Serial.println("[MOBILE-PROFILE] validated profile loaded; scan READY, waiting for START");
                Serial.flush();
            }
        } else if (action == DocumentAnalysisAction::BackToExtra) {
            bleProfileServer.closeSession();
            mapWebEditor.stopDocumentPortal();
            showExtraFeatures();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::Settings) {
        const SettingsAction action = settingsScreen.consumeAction();
        if (action == SettingsAction::TouchDiagnostic) {
            startTouchTest();
        } else if (action == SettingsAction::Wifi) {
            showNetworkScreen(NetworkScreenMode::Wifi);
        } else if (action == SettingsAction::Ethernet) {
            showNetworkScreen(NetworkScreenMode::Ethernet);
        } else if (action == SettingsAction::WorkplaceServer) {
            showWorkplaceServerSettings();
        } else if (action == SettingsAction::HardwareStatus) {
            showBackboneDemo(BackboneModuleId::HardwareValidation,
                             currentP4Language() == P4Language::Turkish ? "DONANIM / PIN FREEZE" : "HARDWARE / PIN FREEZE",
                             BackboneReturn::Settings);
        } else if (action == SettingsAction::Back) {
            showMainMenu();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::WorkplaceServerSettings) {
        const WorkplaceServerSettingsAction action = workplaceServerSettingsScreen.consumeAction();
        if (action == WorkplaceServerSettingsAction::Save) {
            workplaceServerConfig = workplaceServerSettingsScreen.config();
            const bool saved = workplaceServerConfigStore.save(workplaceServerConfig);
            applyWorkplaceServerConfig();
            workplaceServerSettingsScreen.setStorageStatus(workplaceServerConfigStore.statusText());
            const WorkplaceServerConfigValidation validation = validateWorkplaceServerConfig(workplaceServerConfig);
            Serial.printf("[WORKPLACE-CONFIG] save=%s state=%s auth=%s response=%s autoStart=0\n",
                          saved ? "OK" : "FAILED",
                          validation.ready ? "CONFIGURED" : "CONFIG_REQUIRED",
                          workplaceAuthModeName(workplaceServerConfig.authMode),
                          workplaceResponseFormatName(workplaceServerConfig.responseFormat));
            Serial.flush();
        } else if (action == WorkplaceServerSettingsAction::Reset) {
            (void)workplaceServerConfigStore.clear();
            workplaceServerConfig = defaultWorkplaceServerConfig();
            applyWorkplaceServerConfig();
            workplaceServerSettingsScreen.begin(workplaceServerConfig, workplaceServerConfigStore.statusText());
            Serial.println("[WORKPLACE-CONFIG] cleared; state=CONFIG_REQUIRED autoStart=0");
            Serial.flush();
        } else if (action == WorkplaceServerSettingsAction::Back) {
            showSettings();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::Wifi || activeScreen == ActiveScreen::Ethernet) {
        networkStatusScreen.update();
        if (networkStatusScreen.consumeBackRequest()) {
            showSettings();
        }
        networkManager.tick();
        displayPort.update();
        delay(5);
        return;
    }

    if (activeScreen == ActiveScreen::Language) {
        P4Language language = currentP4Language();
        if (languageScreen.consumeSelection(language)) {
            saveLanguage(language);
            showMainMenu();
        } else if (languageScreen.consumeBackRequest()) {
            showMainMenu();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (touchTestActive) {
        if (touchTestScreen.continueRequested()) {
            returnToScanUi();
        }
        displayPort.update();
        delay(5);
        return;
    }

    if (touchControls.consumeMainMenuRequest()) {
        showMainMenu();
        return;
    }

    if (activeScreen == ActiveScreen::Scan && controls.consumeRetestRequest()) {
        handleWorkflowRetestRequest("FRONT_PANEL");
        return;
    }

    if (activeScreen == ActiveScreen::Scan && controls.consumeNextCableRequest()) {
        handleWorkflowNextCableRequest("FRONT_PANEL");
        return;
    }

    if (activeScreen == ActiveScreen::Scan && mapEditRequested) {
        mapEditRequested = false;
        showMapEditor();
        return;
    }

    networkManager.tick();
    scanSession.tick(millis());
    const uint16_t workflowElectricalErrors = static_cast<uint16_t>(
        scanSession.errorCount(ScanDirection::AtoB) +
        scanSession.errorCount(ScanDirection::BtoA));
    testWorkflowController.observeRunState(scanSession.runState(),
                                           workflowElectricalErrors,
                                           scanSession.progressPercent());
    if (scanScreen.update()) {
        // Completion, manual pause and stop-on-error all change the header
        // START/PAUSE label immediately. Do not leave PAUSE on a stopped scan.
        touchControls.refresh();
    }
    const bool finiteScanComplete =
        !scanSession.continuousMode() &&
        scanSession.runState() == RunState::Complete;
    if (finiteScanComplete && !reportPrinted) {
        const uint32_t completionNowMs = millis();
        if (!completionDisplayHoldActive) {
            // The final B->A result has already been rendered into ScanSession.
            // Do not switch to the report in this same loop iteration: that
            // used to make B1/A1 effectively invisible. Give the panel at least
            // one real operator-visible dwell before the report opens.
            completionDisplayHoldActive = true;
            completionDisplayHoldStartedMs = completionNowMs;
        }

        const uint32_t selectedStepMs = scanSession.scanStepIntervalMs();
        const uint32_t finalPointHoldMs =
            selectedStepMs > kFinalScanPointDisplayMinMs
                ? selectedStepMs
                : kFinalScanPointDisplayMinMs;
        if (completionNowMs - completionDisplayHoldStartedMs >= finalPointHoldMs) {
            printDirectionReport(ScanDirection::AtoB);
            printDirectionReport(ScanDirection::BtoA);
            reportPrinted = true;
            completionDisplayHoldActive = false;
            showTestCompletionReport();
            return;
        }
    } else if (!finiteScanComplete) {
        completionDisplayHoldActive = false;
        completionDisplayHoldStartedMs = 0U;
        if (scanSession.runState() != RunState::Complete) {
            reportPrinted = false;
        }
    }
    displayPort.setBacklight(!controls.standby());
    displayPort.update();
    delay(5);
}
