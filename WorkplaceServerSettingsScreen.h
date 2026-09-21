#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "SoundToggleButton.h"
#include "WorkplaceServerConfig.h"

namespace mg::p4 {

enum class WorkplaceServerSettingsAction : uint8_t { None, Save, Reset, Back };

class WorkplaceServerSettingsScreen {
public:
    void begin(const WorkplaceServerConfig& config, const char* storageStatus = nullptr);
    void activate();
    void setStorageStatus(const char* text);
    WorkplaceServerSettingsAction consumeAction();
    const WorkplaceServerConfig& config() const { return config_; }

private:
    enum class Field : uint8_t {
        Endpoint,
        AuthMode,
        AuthUser,
        AuthSecret,
        HeaderName,
        TlsCa,
        ResponseFormat,
        FieldMap,
        Timeout,
        MaxResponse,
        Count
    };
    struct FieldBinding { WorkplaceServerSettingsScreen* owner = nullptr; Field field = Field::Endpoint; };

    static void fieldCallback(lv_event_t* event);
    static void saveCallback(lv_event_t* event);
    static void resetCallback(lv_event_t* event);
    static void backCallback(lv_event_t* event);
    static void keyboardCallback(lv_event_t* event);

    void buildRows();
    void refreshRows();
    void refreshStatus();
    void handleField(Field field);
    void openEditor(Field field);
    void closeEditor(bool commit);
    String fieldText(Field field) const;
    bool applyFieldText(Field field, const String& value, String& error);
    const char* fieldTitle(Field field) const;
    String fieldSummary(Field field) const;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* body_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* rowValue_[static_cast<uint8_t>(Field::Count)]{};
    FieldBinding bindings_[static_cast<uint8_t>(Field::Count)]{};
    SoundToggleButton soundButton_;

    lv_obj_t* editorOverlay_ = nullptr;
    lv_obj_t* editorTitle_ = nullptr;
    lv_obj_t* editorTextArea_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;
    Field editingField_ = Field::Endpoint;

    WorkplaceServerConfig config_{};
    WorkplaceServerSettingsAction pendingAction_ = WorkplaceServerSettingsAction::None;
    String storageStatus_;
};

}  // namespace mg::p4
