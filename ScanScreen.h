#pragma once

#include <lvgl.h>

#include "ConnectorFaceRenderer.h"
#include "DirectionArrow.h"
#include "ScanSession.h"
#include "SoundToggleButton.h"

namespace mg::p4 {

class ScanScreen {
public:
    explicit ScanScreen(ScanSession& session) : session_(session) {}

    using DirectionHandler = void (*)(void* userData);
    enum class ProfileCommand : uint8_t {
        NextSubD,
        DecreasePins,
        IncreasePins,
        NextPinPreset,
        TogglePeA,
        ToggleDrainShieldA,
        TogglePeB,
        ToggleDrainShieldB,
        EditMap,
    };
    using ProfileHandler = void (*)(ProfileCommand command, void* userData);

    void begin();
    void activate();
    lv_obj_t* root() const { return screen_; }
    void onProfileChanged();
    void refreshSoundState() { soundButton_.refresh(); }
    // Returns true only when a dirty session state was rendered. The caller
    // uses this edge to refresh the small header controls without rewriting
    // their labels on every loop iteration.
    bool update(bool force = false);
    void setDirectionHandler(DirectionHandler handler, void* userData) {
        directionHandler_ = handler;
        directionUserData_ = userData;
    }
    void setProfileHandler(ProfileHandler handler, void* userData) {
        profileHandler_ = handler;
        profileUserData_ = userData;
    }

private:
    struct ProfileBinding {
        ScanScreen* owner = nullptr;
        ProfileCommand command = ProfileCommand::NextSubD;
    };

    static void directionArrowCallback(void* userData);
    static void profileButtonCallback(lv_event_t* event);
    static void speedSliderCallback(lv_event_t* event);
    void createTopArea(lv_obj_t* screen);
    void createBottomArea(lv_obj_t* screen);
    void rebuildBars();
    void refreshProfileControls();
    void refreshSpeedControl();
    void refreshLegend();
    void refresh();
    void refreshBars();
    void invalidateBar(uint8_t slot, bool sideA);
    void clearBars();
    void drawBar(uint8_t slot, bool sideA, PointVisualStatus status);
    void drawNumber(uint8_t slot, const char* text, const lv_font_t* font);
    void fillBarsRect(lv_coord_t x,
                      lv_coord_t y,
                      lv_coord_t width,
                      lv_coord_t height,
                      lv_color_t color);
    void setBarsPixel(lv_coord_t x, lv_coord_t y, lv_color_t color);

    static lv_color_t visualColor(PointVisualStatus status);
    void formatTestIndex(uint8_t testIndex,
                         char* output,
                         size_t outputSize) const;
    void formatLiveMask(uint64_t mask,
                        char side,
                        char* output,
                        size_t outputSize) const;
    const char* localizedResultText(ElectricalResult result) const;
    PointVisualStatus liveVisualStatus() const;

    ScanSession& session_;
    ConnectorFaceRenderer connectorA_;
    ConnectorFaceRenderer connectorB_;
    DirectionArrow directionArrow_;
    DirectionHandler directionHandler_ = nullptr;
    void* directionUserData_ = nullptr;
    ProfileHandler profileHandler_ = nullptr;
    void* profileUserData_ = nullptr;
    SoundToggleButton soundButton_;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* profileButton_ = nullptr;
    lv_obj_t* profileLabel_ = nullptr;
    lv_obj_t* normalMinusButton_ = nullptr;
    lv_obj_t* normalCountButton_ = nullptr;
    lv_obj_t* normalCountLabel_ = nullptr;
    lv_obj_t* normalPlusButton_ = nullptr;
    lv_obj_t* peToggleButtonA_ = nullptr;
    lv_obj_t* peToggleLabelA_ = nullptr;
    lv_obj_t* dsToggleButtonA_ = nullptr;
    lv_obj_t* dsToggleLabelA_ = nullptr;
    lv_obj_t* peToggleButtonB_ = nullptr;
    lv_obj_t* peToggleLabelB_ = nullptr;
    lv_obj_t* dsToggleButtonB_ = nullptr;
    lv_obj_t* dsToggleLabelB_ = nullptr;
    lv_obj_t* editMapButton_ = nullptr;
    lv_obj_t* editMapLabel_ = nullptr;
    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* pinAValue_ = nullptr;
    lv_obj_t* pinBValue_ = nullptr;
    lv_obj_t* phaseLabel_ = nullptr;
    lv_obj_t* liveStatusLabel_ = nullptr;
    lv_obj_t* speedLabel_ = nullptr;
    lv_obj_t* speedSlider_ = nullptr;
    lv_obj_t* progressBar_ = nullptr;
    lv_obj_t* progressLabel_ = nullptr;
    lv_obj_t* legendRow_ = nullptr;
    lv_obj_t* legendLabels_[7]{};
    lv_obj_t* barSideBLabel_ = nullptr;
    lv_obj_t* barsCanvas_ = nullptr;
    lv_color_t* barsCanvasBuffer_ = nullptr;
    PointVisualStatus cachedA_[kTestPointsPerSide]{};
    PointVisualStatus cachedB_[kTestPointsPerSide]{};
    lv_coord_t barPitch_ = 0;
    lv_coord_t barWidth_ = 0;
    lv_coord_t barHeight_ = 0;
    lv_coord_t barYA_ = 0;
    lv_coord_t barYB_ = 0;
    uint8_t renderedPointCount_ = 0;
    bool firstRefresh_ = true;
    uint32_t lastUiRefreshMs_ = 0;
    ProfileBinding profileBindings_[9]{};

    static constexpr lv_coord_t kBarsCanvasY = 326;
    static constexpr lv_coord_t kBarsCanvasWidth = kBarAreaWidth;
    static constexpr lv_coord_t kBarsCanvasHeight = 239;
};

}  // namespace mg::p4
