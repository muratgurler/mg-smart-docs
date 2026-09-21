#include "FeatureBackbone.h"

#include <stdio.h>
#include <string.h>

namespace mg::p4 {
namespace {

uint8_t moduleIndex(BackboneModuleId module) {
    return static_cast<uint8_t>(module);
}

void copyText(char* dst, size_t dstSize, const char* src) {
    if (dstSize == 0U) return;
    snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

}  // namespace

void FeatureBackbone::begin(v33::V33HardwareManager& hardware) {
    hardware_ = &hardware;
    preferHardware_ = false;
    select(BackboneModuleId::Scan128, nullptr);
    Serial.println("[BACKBONE] 15-section software backbone ready; DEMO fallback active");
    Serial.flush();
}

void FeatureBackbone::attachDriver(BackboneModuleId module, BackboneModuleDriver* driver) {
    const uint8_t index = moduleIndex(module);
    if (index >= kModuleCount) return;
    drivers_[index] = driver;
}

BackboneModuleDriver* FeatureBackbone::activeDriver() const {
    if (!preferHardware_) return nullptr;
    const uint8_t index = moduleIndex(selectedModule_);
    if (index >= kModuleCount) return nullptr;
    BackboneModuleDriver* driver = drivers_[index];
    return driver != nullptr && driver->available() ? driver : nullptr;
}

bool FeatureBackbone::driverUsable(BackboneModuleId module) const {
    const uint8_t index = moduleIndex(module);
    if (index >= kModuleCount) return false;
    return drivers_[index] != nullptr && drivers_[index]->available();
}

void FeatureBackbone::select(BackboneModuleId module, const char* workflowTitle) {
    selectedModule_ = module;
    copyText(workflowTitle_, sizeof(workflowTitle_), workflowTitle);
    if (BackboneModuleDriver* driver = activeDriver()) {
        driver->reset();
    }
    resetDemo();
}

void FeatureBackbone::start() {
    if (BackboneModuleDriver* driver = activeDriver()) {
        const BackboneSnapshot snap = driver->snapshot();
        if (snap.state == BackboneRunState::Paused) driver->resume();
        else driver->start();
        return;
    }
    if (demoState_ == BackboneRunState::Complete || demoState_ == BackboneRunState::Fault) {
        resetDemo();
    }
    demoState_ = BackboneRunState::Running;
    lastDemoStepMs_ = millis();
    refreshDemoMetrics();
}

void FeatureBackbone::pauseResume() {
    if (BackboneModuleDriver* driver = activeDriver()) {
        const BackboneSnapshot snap = driver->snapshot();
        if (snap.state == BackboneRunState::Running) driver->pause();
        else if (snap.state == BackboneRunState::Paused) driver->resume();
        else driver->start();
        return;
    }
    if (demoState_ == BackboneRunState::Running) demoState_ = BackboneRunState::Paused;
    else if (demoState_ == BackboneRunState::Paused) {
        demoState_ = BackboneRunState::Running;
        lastDemoStepMs_ = millis();
    } else {
        start();
    }
    refreshDemoMetrics();
}

void FeatureBackbone::step() {
    if (BackboneModuleDriver* driver = activeDriver()) {
        driver->step();
        return;
    }
    if (demoState_ == BackboneRunState::Complete) resetDemo();
    demoState_ = BackboneRunState::Paused;
    advanceDemo();
}

void FeatureBackbone::reset() {
    if (BackboneModuleDriver* driver = activeDriver()) driver->reset();
    resetDemo();
}

void FeatureBackbone::tick(uint32_t nowMs) {
    if (BackboneModuleDriver* driver = activeDriver()) {
        driver->tick(nowMs);
        return;
    }
    if (demoState_ != BackboneRunState::Running) return;
    if (nowMs - lastDemoStepMs_ < 700U) return;
    lastDemoStepMs_ = nowMs;
    advanceDemo();
}

uint8_t FeatureBackbone::demoPhaseCount(BackboneModuleId module) const {
    switch (module) {
        case BackboneModuleId::Scan128: return 5;
        case BackboneModuleId::Kelvin: return 5;
        case BackboneModuleId::SelfTestCalibration: return 6;
        case BackboneModuleId::Tdr: return 5;
        case BackboneModuleId::PairIntegrity: return 5;
        case BackboneModuleId::UsbC: return 6;
        case BackboneModuleId::PeDs: return 5;
        case BackboneModuleId::FixtureSpc: return 5;
        case BackboneModuleId::SmartProbe: return 4;
        case BackboneModuleId::Network: return 5;
        case BackboneModuleId::Voice: return 4;
        case BackboneModuleId::EnvironmentAwg: return 4;
        case BackboneModuleId::BarcodeQr: return 5;
        case BackboneModuleId::ComponentTest: return 5;
        case BackboneModuleId::HardwareValidation: return 6;
        case BackboneModuleId::CableLearn: return 5;
        case BackboneModuleId::MultiConnector: return 6;
        case BackboneModuleId::FlexGlitch: return 5;
        case BackboneModuleId::Count: break;
    }
    return 1;
}

void FeatureBackbone::resetDemo() {
    demoState_ = BackboneRunState::Idle;
    demoPhaseIndex_ = 0;
    lastDemoStepMs_ = millis();
    demoSnapshot_ = BackboneSnapshot{};
    demoSnapshot_.module = selectedModule_;
    demoSnapshot_.state = demoState_;
    demoSnapshot_.phaseCount = demoPhaseCount(selectedModule_);
    demoSnapshot_.usingDemo = true;
    demoSnapshot_.hardwareAvailable = driverUsable(selectedModule_);
    refreshDemoMetrics();
}

void FeatureBackbone::advanceDemo() {
    const uint8_t count = demoPhaseCount(selectedModule_);
    if (demoPhaseIndex_ + 1U >= count) {
        demoPhaseIndex_ = static_cast<uint8_t>(count - 1U);
        demoState_ = BackboneRunState::Complete;
    } else {
        ++demoPhaseIndex_;
    }
    refreshDemoMetrics();
}

void FeatureBackbone::refreshDemoMetrics() {
    const uint8_t count = demoPhaseCount(selectedModule_);
    demoSnapshot_.module = selectedModule_;
    demoSnapshot_.state = demoState_;
    demoSnapshot_.phaseIndex = demoPhaseIndex_;
    demoSnapshot_.phaseCount = count;
    demoSnapshot_.progressPercent = count <= 1U ? 100U
        : static_cast<uint8_t>((static_cast<uint16_t>(demoPhaseIndex_) * 100U) /
                               static_cast<uint16_t>(count - 1U));
    demoSnapshot_.usingDemo = activeDriver() == nullptr;
    demoSnapshot_.hardwareAvailable = driverUsable(selectedModule_);

    demoSnapshot_.metric1[0] = '\0';
    demoSnapshot_.metric2[0] = '\0';
    demoSnapshot_.metric3[0] = '\0';

    switch (selectedModule_) {
        case BackboneModuleId::Scan128:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Nodes: 64 A + 64 B");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Sender LOW / other 127 INPUT");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "A8->B9 demo fault");
            break;
        case BackboneModuleId::Kelvin:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "I=%u mA", demoPhaseIndex_ < 2U ? 10U : (demoPhaseIndex_ < 4U ? 50U : 100U));
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "R=8.%u mOhm", static_cast<unsigned>(2U + demoPhaseIndex_));
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "dR=+0.%u mOhm", static_cast<unsigned>(demoPhaseIndex_));
            break;
        case BackboneModuleId::SelfTestCalibration:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "FAST/FULL refs");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "10m / 100m / 1R");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "CRC calibration data");
            break;
        case BackboneModuleId::Tdr:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Selected: A17");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "t=51.%u ns", static_cast<unsigned>(demoPhaseIndex_));
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Fault ~= 5.2 m");
            break;
        case BackboneModuleId::PairIntegrity:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "20/50/100/250 kHz");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Xtalk=-31 dB");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Split pair: NONE");
            break;
        case BackboneModuleId::UsbC:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "SOP' Discover Identity");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Cable: 5A / EPR");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "VBUS safe <=150 mA");
            break;
        case BackboneModuleId::PeDs:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "PE: 6.8 mOhm");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "dS: single/bond profile");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Bond=PROFILE CONTROLLED");
            break;
        case BackboneModuleId::FixtureSpc:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Fixture health: 96%%");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Cpk=1.42 demo");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Pin A12 drift +0.6mOhm");
            break;
        case BackboneModuleId::SmartProbe:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Outputs: SAFE/HIGH-Z");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Search: remaining 127");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Found net: A7,B9,B12");
            break;
        case BackboneModuleId::Network:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Ethernet/Wi-Fi STA");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "PR -> HTTP GET");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Profile READY / no autostart");
            break;
        case BackboneModuleId::Voice:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Wake: Hi MG");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Command: START/PAUSE");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Same state-machine action");
            break;
        case BackboneModuleId::EnvironmentAwg:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "T=23.6 C / RH=48%%");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "R_RAW preserved");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "AWG/mm2 plausibility only");
            break;
        case BackboneModuleId::BarcodeQr:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "PR / ASML / MG profile");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Lookup -> validate -> load");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "READY / START required");
            break;
        case BackboneModuleId::ComponentTest:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Diode/LED direction");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Basic capacitor presence");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "No precision LCR claim");
            break;
        case BackboneModuleId::HardwareValidation:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Pin Freeze: CANDIDATE");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "GPIO 1/2/3/4/20/32/33");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "GPIO45/46/47 + GPIO5 reserved");
            break;
        case BackboneModuleId::CableLearn:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Learn connected components");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "1:N / N:N / same-side");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Draft map -> operator save");
            break;
        case BackboneModuleId::MultiConnector:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Segments: A-B / B-C / C-D");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "No intermediate bypass");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Per-segment map/profile");
            break;
        case BackboneModuleId::FlexGlitch:
            snprintf(demoSnapshot_.metric1, sizeof(demoSnapshot_.metric1), "Comparator + HW latch");
            snprintf(demoSnapshot_.metric2, sizeof(demoSnapshot_.metric2), "Cable move window active");
            snprintf(demoSnapshot_.metric3, sizeof(demoSnapshot_.metric3), "Glitch count: %u", static_cast<unsigned>(demoPhaseIndex_ == 3U ? 1U : 0U));
            break;
        case BackboneModuleId::Count:
            break;
    }
}

BackboneSnapshot FeatureBackbone::snapshot() const {
    if (BackboneModuleDriver* driver = activeDriver()) {
        BackboneSnapshot snap = driver->snapshot();
        snap.usingDemo = false;
        snap.hardwareAvailable = true;
        return snap;
    }
    return demoSnapshot_;
}

const char* FeatureBackbone::stateText(BackboneRunState state, bool turkish) {
    switch (state) {
        case BackboneRunState::Idle: return turkish ? "HAZIR" : "READY";
        case BackboneRunState::Running: return turkish ? "CALISIYOR" : "RUNNING";
        case BackboneRunState::Paused: return turkish ? "DURAKLATILDI" : "PAUSED";
        case BackboneRunState::Complete: return turkish ? "TAMAMLANDI" : "COMPLETE";
        case BackboneRunState::Fault: return turkish ? "HATA" : "FAULT";
    }
    return "?";
}

const char* FeatureBackbone::moduleTitle(BackboneModuleId module, bool tr) {
    switch (module) {
        case BackboneModuleId::Scan128: return tr ? "128-NODE TARAMA" : "128-NODE SCAN";
        case BackboneModuleId::Kelvin: return tr ? "KELVIN DIRENC / KRIMP" : "KELVIN RESISTANCE / CRIMP";
        case BackboneModuleId::SelfTestCalibration: return tr ? "SELF TEST / KALIBRASYON" : "SELF TEST / CALIBRATION";
        case BackboneModuleId::Tdr: return tr ? "TDR KOPUK YERI" : "TDR FAULT LOCATION";
        case BackboneModuleId::PairIntegrity: return tr ? "PAIR INTEGRITY" : "PAIR INTEGRITY";
        case BackboneModuleId::UsbC: return tr ? "USB-C E-MARKER / PD" : "USB-C E-MARKER / PD";
        case BackboneModuleId::PeDs: return tr ? "PE / dS KALITESI" : "PE / dS QUALITY";
        case BackboneModuleId::FixtureSpc: return tr ? "FIXTURE HEALTH / SPC" : "FIXTURE HEALTH / SPC";
        case BackboneModuleId::SmartProbe: return tr ? "SMART PROBE" : "SMART PROBE";
        case BackboneModuleId::Network: return tr ? "AG / PR LOOKUP" : "NETWORK / PR LOOKUP";
        case BackboneModuleId::Voice: return tr ? "SES / KOMUT" : "VOICE / COMMAND";
        case BackboneModuleId::EnvironmentAwg: return tr ? "ORTAM / AWG" : "ENVIRONMENT / AWG";
        case BackboneModuleId::BarcodeQr: return tr ? "BARCODE / QR" : "BARCODE / QR";
        case BackboneModuleId::ComponentTest: return tr ? "DIYOT / LED / KAPASITOR" : "DIODE / LED / CAPACITOR";
        case BackboneModuleId::HardwareValidation: return tr ? "PCB / PIN FREEZE" : "PCB / PIN FREEZE";
        case BackboneModuleId::CableLearn: return tr ? "KABLO OGRENME" : "CABLE LEARN";
        case BackboneModuleId::MultiConnector: return tr ? "COKLU KONNEKTOR" : "MULTI-CONNECTOR";
        case BackboneModuleId::FlexGlitch: return tr ? "ANLIK TEMASSIZLIK" : "INTERMITTENT CONTACT";
        case BackboneModuleId::Count: break;
    }
    return "MODULE";
}

const char* FeatureBackbone::moduleDescription(BackboneModuleId module, bool tr) {
    switch (module) {
        case BackboneModuleId::Scan128:
            return tr ? "Bir sender LOW iken kalan 127 node okunur; open/short/wrong/same-side/common-net siniflandirilir." : "Drive one sender LOW and read the other 127 nodes; classify open/short/wrong/same-side/common nets.";
        case BackboneModuleId::Kelvin:
            return tr ? "ADS122C04 tabanli 10/50/100 mA ratiometric Kelvin akisi; simdilik demo olcum motoru." : "ADS122C04 based 10/50/100 mA ratiometric Kelvin flow; demo measurement engine for now.";
        case BackboneModuleId::SelfTestCalibration:
            return tr ? "FAST/FULL self-test, precision referanslar ve servis kalibrasyon omurgasi." : "FAST/FULL self-test, precision references and service calibration backbone.";
        case BackboneModuleId::Tdr:
            return tr ? "A tarafindan secili hatta yaklasik ariza mesafesi; VOP kalibrasyonu ile." : "Approximate fault distance from side A with VOP calibration.";
        case BackboneModuleId::PairIntegrity:
            return tr ? "Bukumlu cift, split-pair, genlik/faz/crosstalk kontrol akisi." : "Twisted-pair, split-pair, amplitude/phase/crosstalk workflow.";
        case BackboneModuleId::UsbC:
            return tr ? "FUSB302B, VCONN, SOP' Discover Identity ve E-Marker profil karsilastirmasi." : "FUSB302B, VCONN, SOP' Discover Identity and E-Marker profile comparison.";
        case BackboneModuleId::PeDs:
            return tr ? "PE ve drain/shield ayri node; REQUIRED/ALLOWED/FORBIDDEN bond ve Kelvin kalite." : "PE and drain/shield as separate nodes; bond policy plus Kelvin quality.";
        case BackboneModuleId::FixtureSpc:
            return tr ? "Fixture baseline, pin drift, tekrar eden ariza ve SPC trend omurgasi." : "Fixture baseline, pin drift, repeated-fault and SPC trend backbone.";
        case BackboneModuleId::SmartProbe:
            return tr ? "Tum cikislar guvenli input; probla bulunan elektriksel neti gosterir." : "All outputs safe/high-Z; probe identifies the connected electrical net.";
        case BackboneModuleId::Network:
            return tr ? "Ethernet/Wi-Fi, PR HTTP GET, referans/profile resolve; serbest test agdan bagimsiz." : "Ethernet/Wi-Fi, PR HTTP GET and profile resolve; free test remains network independent.";
        case BackboneModuleId::Voice:
            return tr ? "Hi MG wake-word ve mevcut state-machine komutlarina guvenli yonlendirme." : "Hi MG wake-word and safe routing into existing state-machine actions.";
        case BackboneModuleId::EnvironmentAwg:
            return tr ? "SHT40 logging, R_RAW korumasi ve opsiyonel kesit/uzunluk uygunluk kontrolu." : "SHT40 logging, preserved R_RAW and optional conductor-size/length plausibility.";
        case BackboneModuleId::BarcodeQr:
            return tr ? "PR/ASML/MG kodunu tanir, profili yukler ve READY'de START bekler." : "Classifies PR/ASML/MG codes, loads the profile and waits in READY for START.";
        case BackboneModuleId::ComponentTest:
            return tr ? "Dusuk maliyetli temel diyot/LED/polarite ve kapasitor varlik testi." : "Low-cost basic diode/LED/polarity and capacitor presence checks.";
        case BackboneModuleId::HardwareValidation:
            return tr ? "Gercek PCB gelince GPIO boot-state, continuity ve bus hiz dogrulama kapisi." : "Gate for GPIO boot-state, continuity and bus-speed validation when the PCB arrives.";
        case BackboneModuleId::CableLearn:
            return tr ? "Fiziksel baglantilari connected-component net modeliyle ogrenir ve harita taslagi olusturur." : "Learns physical connections as electrical connected components and creates a map draft.";
        case BackboneModuleId::MultiConnector:
            return tr ? "A-B, B-C, C-D gibi bitisik segmentleri ayri profillerle test eder; bypass kabul etmez." : "Tests adjacent A-B, B-C, C-D segments with per-segment maps; no bypass allowed.";
        case BackboneModuleId::FlexGlitch:
            return tr ? "Comparator + hardware latch ile kablo hareketindeki kisa temas kayiplarini yakalar." : "Comparator plus hardware latch catches short contact dropouts during cable movement.";
        case BackboneModuleId::Count: break;
    }
    return "";
}

const char* FeatureBackbone::phaseText(BackboneModuleId module, uint8_t phase, bool tr) {
    switch (module) {
        case BackboneModuleId::Scan128: {
            static const char* en[] = {"SAFE INPUT SETUP", "A -> B SWEEP", "B -> A SWEEP", "NET CLASSIFICATION", "REPORT READY"};
            static const char* tt[] = {"GUVENLI INPUT HAZIRLA", "A -> B TARAMA", "B -> A TARAMA", "NET SINIFLANDIRMA", "RAPOR HAZIR"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::Kelvin: {
            static const char* en[] = {"OFFSET / ZERO", "10 mA SAMPLE", "50 mA SAMPLE", "100 mA CONFIRM", "DELTA-R EVALUATION"};
            static const char* tt[] = {"OFFSET / ZERO", "10 mA OLCUM", "50 mA OLCUM", "100 mA DOGRULAMA", "DELTA-R DEGERLENDIR"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::SelfTestCalibration: {
            static const char* en[] = {"P4 / MEMORY / BUS", "MCP23S17 READBACK", "KELVIN SHORT", "10m/100m/1R REFS", "TDR / PAIR / USB-C", "SELF TEST PASS"};
            static const char* tt[] = {"P4 / BELLEK / BUS", "MCP23S17 READBACK", "KELVIN SHORT", "10m/100m/1R REF", "TDR / PAIR / USB-C", "SELF TEST PASS"};
            return (tr ? tt : en)[phase % 6U];
        }
        case BackboneModuleId::Tdr: {
            static const char* en[] = {"SELECT A NODE", "ARM TDC7200", "LAUNCH PULSE", "CAPTURE REFLECTION", "DISTANCE RESULT"};
            static const char* tt[] = {"A NODE SEC", "TDC7200 HAZIRLA", "PULSE GONDER", "YANSIMA YAKALA", "MESAFE SONUCU"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::PairIntegrity: {
            static const char* en[] = {"ROUTE TX/RX PAIR", "20/50 kHz", "100 kHz", "250 kHz", "PAIR RESULT"};
            static const char* tt[] = {"TX/RX CIFT SEC", "20/50 kHz", "100 kHz", "250 kHz", "CIFT SONUCU"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::UsbC: {
            static const char* en[] = {"ATTACH", "ORIENTATION", "VBUS SAFE", "VCONN", "SOP' IDENTITY", "PROFILE RESULT"};
            static const char* tt[] = {"ATTACH", "YON BUL", "VBUS SAFE", "VCONN", "SOP' IDENTITY", "PROFIL SONUCU"};
            return (tr ? tt : en)[phase % 6U];
        }
        case BackboneModuleId::PeDs: {
            static const char* en[] = {"PE CONTINUITY", "dS TOPOLOGY", "BOND POLICY", "KELVIN QUALITY", "PE/dS RESULT"};
            static const char* tt[] = {"PE SUREKLILIK", "dS TOPOLOJI", "BOND KURALI", "KELVIN KALITE", "PE/dS SONUCU"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::FixtureSpc: {
            static const char* en[] = {"LOAD BASELINE", "CHECK PIN DRIFT", "CHECK REPEATS", "SPC TREND", "PROCESS STATUS"};
            static const char* tt[] = {"BASELINE YUKLE", "PIN DRIFT KONTROL", "TEKRAR HATA KONTROL", "SPC TREND", "PROSES DURUMU"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::SmartProbe: {
            static const char* en[] = {"ALL HIGH-Z", "SCAN CANDIDATES", "VERIFY CONTACT", "NET FOUND"};
            static const char* tt[] = {"TUMU HIGH-Z", "ADAYLARI TARA", "TEMASI DOGRULA", "NET BULUNDU"};
            return (tr ? tt : en)[phase % 4U];
        }
        case BackboneModuleId::Network: {
            static const char* en[] = {"LINK", "REQUEST PR", "RESOLVE REFERENCE", "LOAD PROFILE", "READY / NO AUTOSTART"};
            static const char* tt[] = {"BAGLANTI", "PR SORGULA", "REFERANS COZ", "PROFIL YUKLE", "READY / AUTO START YOK"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::Voice: {
            static const char* en[] = {"WAIT HI MG", "LISTEN COMMAND", "VALIDATE ACTION", "STATE MACHINE ACTION"};
            static const char* tt[] = {"HI MG BEKLE", "KOMUT DINLE", "KOMUTU DOGRULA", "STATE MACHINE CALISTIR"};
            return (tr ? tt : en)[phase % 4U];
        }
        case BackboneModuleId::EnvironmentAwg: {
            static const char* en[] = {"READ SHT40", "LOG RAW R + TEMP", "WIRE-SPEC CHECK", "CONTEXT READY"};
            static const char* tt[] = {"SHT40 OKU", "RAW R + SICAKLIK KAYDET", "WIRE-SPEC KONTROL", "ORTAM VERISI HAZIR"};
            return (tr ? tt : en)[phase % 4U];
        }
        case BackboneModuleId::BarcodeQr: {
            static const char* en[] = {"WAIT CODE", "CLASSIFY CODE", "LOOKUP PROFILE", "VALIDATE PROFILE", "READY / START REQUIRED"};
            static const char* tt[] = {"KOD BEKLE", "KODU SINIFLANDIR", "PROFIL ARA", "PROFILI DOGRULA", "READY / START GEREKLI"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::ComponentTest: {
            static const char* en[] = {"SAFE BIAS", "DIODE DIRECTION", "LED PRESENCE", "CAPACITOR BASIC", "COMPONENT RESULT"};
            static const char* tt[] = {"GUVENLI BIAS", "DIYOT YONU", "LED VARLIK", "KAPASITOR TEMEL", "KOMPONENT SONUCU"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::HardwareValidation: {
            static const char* en[] = {"JC1060 CONTINUITY", "BOOT-STATE GPIO", "SPI/I2C BUS", "10/8/5 MHz CHECK", "SAFE OUTPUT CHECK", "PIN FREEZE CANDIDATE"};
            static const char* tt[] = {"JC1060 CONTINUITY", "BOOT-STATE GPIO", "SPI/I2C BUS", "10/8/5 MHz KONTROL", "GUVENLI OUTPUT KONTROL", "PIN FREEZE ADAYI"};
            return (tr ? tt : en)[phase % 6U];
        }
        case BackboneModuleId::CableLearn: {
            static const char* en[] = {"SAFE SCAN", "DISCOVER NETS", "MERGE COMPONENTS", "BUILD MAP DRAFT", "WAIT OPERATOR SAVE"};
            static const char* tt[] = {"GUVENLI TARAMA", "NETLERI BUL", "ORTAK NETLERI BIRLESTIR", "HARITA TASLAGI", "OPERATOR KAYDI BEKLE"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::MultiConnector: {
            static const char* en[] = {"LOAD SEGMENTS", "A-B", "B-C", "C-D", "NO-BYPASS CHECK", "SEGMENT REPORT"};
            static const char* tt[] = {"SEGMENTLERI YUKLE", "A-B", "B-C", "C-D", "BYPASS KONTROL", "SEGMENT RAPORU"};
            return (tr ? tt : en)[phase % 6U];
        }
        case BackboneModuleId::FlexGlitch: {
            static const char* en[] = {"ARM LATCH", "MOVE CABLE", "WATCH COMPARATOR", "GLITCH CAPTURE", "EVENT RESULT"};
            static const char* tt[] = {"LATCH HAZIRLA", "KABLOYU HAREKET ETTIR", "KOMPARATOR IZLE", "GLITCH YAKALA", "OLAY SONUCU"};
            return (tr ? tt : en)[phase % 5U];
        }
        case BackboneModuleId::Count:
            break;
    }
    return "";
}

}  // namespace mg::p4
