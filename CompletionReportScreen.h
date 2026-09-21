#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableNetGraph.h"
#include "CableProfile.h"
#include "ScanSession.h"
#include "TestWorkflowController.h"
#include "MgResultTable.h"

namespace mg::p4 {

enum class CompletionReportKind : uint8_t {
    Test = 0,
    CableLearn,
};

enum class CompletionReportAction : uint8_t {
    None = 0,
    MainMenu,
    Retest,
    Graph,
    Records,
    SaveLearnedProfile,
    ReturnToLearning,
};

// Full-screen completion report used by finite cable tests and Cable Learn.
// NON-STOP scans never enter this screen; their ScanSession deliberately has
// no terminal cycle. The current UI replaces page buttons with one vertically scrollable
// table while keeping the header and operator action bar fixed.
class CompletionReportScreen {
public:
    void beginTest(TestWorkflowController& workflow, const ScanSession& session);
    void beginCableLearn(const CableProfile& profile,
                         const CableMap& learnedMap,
                         size_t learnedNetCount,
                         bool demoMode);
    void activate();
    void refresh();
    CompletionReportAction consumeAction();
    void markLearnedProfileSaved(bool success);

    CompletionReportKind kind() const { return kind_; }
    const CableProfile& learnedProfile() const { return learnedProfile_; }
    const CableMap& learnedMap() const { return learnedMap_; }

private:
    struct Binding {
        CompletionReportScreen* owner = nullptr;
        CompletionReportAction action = CompletionReportAction::None;
    };
    struct TestEntry {
        ScanDirection direction = ScanDirection::AtoB;
        uint8_t slot = 0U;
    };

    static void buttonCallback(lv_event_t* event);
    void buildUi();
    void configureReportTables();
    void rebuildTestEntries();
    void rebuildLearnedNets();
    void refreshHeader();
    void refreshTable();
    lv_obj_t* makeButton(lv_obj_t* parent,
                         lv_coord_t x,
                         lv_coord_t y,
                         lv_coord_t w,
                         const char* text,
                         lv_color_t color,
                         uint8_t bindingIndex,
                         CompletionReportAction action);
    static void copyText(char* dst, size_t dstSize, const char* src);
    static void formatNetMask(uint64_t mask,
                              char side,
                              char* output,
                              size_t outputSize);
    static const char* learnedNetType(const CableNet& net);
    static MgResultStatus resultRowStatus(ElectricalResult result);

    CompletionReportKind kind_ = CompletionReportKind::Test;
    TestWorkflowController* workflow_ = nullptr;
    const ScanSession* session_ = nullptr;

    CableProfile learnedProfile_{};
    CableMap learnedMap_{};
    char learnedProfileName_[48]{};
    char learnedConnectorA_[40]{};
    char learnedConnectorB_[40]{};
    CableNet learnedNets_[kTestPointsPerSide]{};
    size_t learnedNetCount_ = 0U;
    bool learnedDemoMode_ = false;
    bool learnedProfileSaved_ = false;

    TestEntry testEntries_[kTestPointsPerSide * 2U]{};
    size_t testEntryCount_ = 0U;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* titleLabel_ = nullptr;
    lv_obj_t* verdictLabel_ = nullptr;
    lv_obj_t* identityLabel_ = nullptr;
    lv_obj_t* countsLabel_ = nullptr;
    lv_obj_t* detailsTitleLabel_ = nullptr;
    MgResultTable resultTable_{};
    lv_obj_t* actionButtons_[5]{};
    lv_obj_t* actionLabels_[5]{};
    Binding bindings_[5]{};
    CompletionReportAction pendingAction_ = CompletionReportAction::None;

    static constexpr uint16_t kTableColumnCount = 7U;
};

}  // namespace mg::p4
