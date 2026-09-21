#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace mg::p4 {

class MapWebEditor;

enum class DocumentReviewAction : uint8_t {
    None,
    ReturnToUpload,
    PrepareAnalysis,
    Back,
};

class DocumentReviewScreen {
public:
    void begin(MapWebEditor& portal);
    void activate();
    DocumentReviewAction consumeAction();

    static void bottomCallback(lv_event_t* event);
    static void rowCallback(lv_event_t* event);

private:
    enum class RowAction : int8_t {
        Up = -1,
        Down = 1,
        Delete = 2,
    };

    struct RowContext {
        DocumentReviewScreen* self = nullptr;
        uint8_t index = 0U;
        RowAction action = RowAction::Delete;
    };

    struct BottomContext {
        DocumentReviewScreen* self = nullptr;
        DocumentReviewAction action = DocumentReviewAction::None;
    };

    void buildUi();
    void rebuildList();
    void refreshSummary();
    void handleRow(uint8_t index, RowAction action);

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* summaryLabel_ = nullptr;
    lv_obj_t* listPanel_ = nullptr;
    lv_obj_t* prepareButton_ = nullptr;

    MapWebEditor* portal_ = nullptr;
    DocumentReviewAction action_ = DocumentReviewAction::None;
    RowContext rowContexts_[64][3];
    BottomContext bottomContexts_[3];
};

}  // namespace mg::p4
