#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableProfile.h"
#include "SoundToggleButton.h"

namespace mg::p4 {

enum class MapEditorAction : uint8_t {
    None,
    Save,
    Test,
    Graph,
    Back,
    DocumentDone,
};

enum class MapEditorContext : uint8_t {
    Normal,
    DocumentReview,
};

enum class MapEditMode : uint8_t {
    AtoB,
    SameA,
    SameB,
};

class MapEditorScreen {
public:
    void begin(const CableMap& source, const CableProfile& profile);
    void begin(const CableMap& source, const CableProfile& profile,
               MapEditorContext context);
    void activate();
    void reloadFrom(const CableMap& source);
    void markSaved();
    void focusPoint(MapEditMode mode, uint8_t sourceIndex);

    MapEditorAction consumeAction();
    const CableMap& workingMap() const { return working_; }
    bool dirty() const { return dirty_; }
    MapEditorContext context() const { return context_; }

private:
    struct PinBinding {
        MapEditorScreen* owner = nullptr;
        bool targetPane = false;
        uint8_t index = 0;
    };
    struct ModeBinding {
        MapEditorScreen* owner = nullptr;
        MapEditMode mode = MapEditMode::AtoB;
    };
    enum class LocalAction : uint8_t { OneToOne, Clear, Graph, Save, Test, Back, UnlockEdit };
    struct ActionBinding {
        MapEditorScreen* owner = nullptr;
        LocalAction action = LocalAction::Back;
    };

    static void pinCallback(lv_event_t* event);
    static void modeCallback(lv_event_t* event);
    static void actionCallback(lv_event_t* event);

    void build();
    void refresh();
    void selectSource(uint8_t index);
    void toggleTarget(uint8_t index);
    void setMode(MapEditMode mode);
    void handleAction(LocalAction action);
    void formatPin(uint8_t index, char* output, size_t outputSize) const;
    void formatTargets(char* output, size_t outputSize) const;
    uint64_t currentTargetMask() const;
    void rebuildVisiblePoints();
    bool isVisiblePoint(uint8_t index) const;
    bool pointEnabledForSide(char side, uint8_t index) const;
    uint8_t firstEnabledVisiblePoint(char side) const;
    char sourceSide() const;
    char targetSide() const;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* sourceButtons_[kTestPointsPerSide]{};
    lv_obj_t* targetButtons_[kTestPointsPerSide]{};
    lv_obj_t* modeButtons_[3]{};
    lv_obj_t* sourcePaneTitle_ = nullptr;
    lv_obj_t* targetPaneTitle_ = nullptr;
    lv_obj_t* subtitleLabel_ = nullptr;
    lv_obj_t* selectedLabel_ = nullptr;
    lv_obj_t* targetsLabel_ = nullptr;
    lv_obj_t* dirtyLabel_ = nullptr;
    lv_obj_t* instructionLabel_ = nullptr;

    PinBinding sourceBindings_[kTestPointsPerSide]{};
    PinBinding targetBindings_[kTestPointsPerSide]{};
    ModeBinding modeBindings_[3]{};
    ActionBinding actionBindings_[6]{};

    SoundToggleButton soundButton_;
    CableMap working_;
    CableProfile profile_ = makeNormalProfile();
    uint8_t visibleIndices_[kTestPointsPerSide]{};
    uint8_t visibleCount_ = 0U;
    uint64_t visibleMask_ = 0ULL;
    MapEditMode mode_ = MapEditMode::AtoB;
    MapEditorContext context_ = MapEditorContext::Normal;
    uint8_t sourceIndex_ = 1U;
    bool dirty_ = false;
    bool editUnlocked_ = true;
    MapEditorAction pendingAction_ = MapEditorAction::None;
};

}  // namespace mg::p4
