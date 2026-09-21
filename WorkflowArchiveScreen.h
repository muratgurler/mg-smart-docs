#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "WorkflowArchiveCore.h"

namespace mg::p4 {

enum class WorkflowArchiveAction : uint8_t {
    None = 0,
    LoadSelectedProfile,
    ExportSelectedResultCsv,
    ExportAllResultsCsv,
    Back,
};

class WorkflowArchiveScreen {
public:
    void begin(WorkflowArchiveCore& archive);
    void activate();
    void update();
    WorkflowArchiveAction consumeAction();
    size_t selectedProfileOffset() const { return selectedOffset_; }
    size_t selectedResultOffset() const { return selectedOffset_; }
    void setStorageStatus(const char* text);

private:
    enum class ViewMode : uint8_t { Summary = 0, Results, Profiles };
    enum class ButtonCommand : uint8_t {
        Summary,
        Results,
        Profiles,
        Previous,
        Next,
        Primary,
        Back,
    };

    struct Binding {
        WorkflowArchiveScreen* owner = nullptr;
        ButtonCommand command = ButtonCommand::Summary;
    };

    static void buttonCallback(lv_event_t* event);
    void handle(ButtonCommand command);
    void buildUi();
    void refresh();
    lv_obj_t* makeButton(lv_obj_t* parent,
                         lv_coord_t x,
                         lv_coord_t y,
                         lv_coord_t width,
                         const char* text,
                         lv_color_t color,
                         uint8_t bindingIndex,
                         ButtonCommand command,
                         lv_obj_t** captionOut = nullptr);

    WorkflowArchiveCore* archive_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* modeLabel_ = nullptr;
    lv_obj_t* counterLabel_ = nullptr;
    lv_obj_t* primaryLabel_ = nullptr;
    lv_obj_t* secondaryLabel_ = nullptr;
    lv_obj_t* identityLabel_ = nullptr;
    lv_obj_t* detailLabel_ = nullptr;
    lv_obj_t* reportLabel_ = nullptr;
    lv_obj_t* storageLabel_ = nullptr;
    lv_obj_t* previousButton_ = nullptr;
    lv_obj_t* nextButton_ = nullptr;
    lv_obj_t* primaryButton_ = nullptr;
    lv_obj_t* primaryButtonLabel_ = nullptr;
    Binding bindings_[7]{};
    ViewMode mode_ = ViewMode::Summary;
    size_t selectedOffset_ = 0U;
    WorkflowArchiveAction pendingAction_ = WorkflowArchiveAction::None;
    char storageStatus_[96] = "LittleFS status pending";
};

}  // namespace mg::p4
