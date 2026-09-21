#pragma once

#include <lvgl.h>

#include "ScanSource.h"

namespace mg::p4 {

class DirectionArrow {
public:
    using ClickHandler = void (*)(void* userData);

    void create(lv_obj_t* parent,
                lv_coord_t x,
                lv_coord_t y,
                ClickHandler clickHandler = nullptr,
                void* clickUserData = nullptr);
    void setDirection(ScanDirection direction, lv_color_t color);

private:
    static void eventCallback(lv_event_t* event);
    void redraw();

    static constexpr lv_coord_t kWidth = 236;
    static constexpr lv_coord_t kHeight = 92;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    lv_color_t* canvasBuffer_ = nullptr;
    ScanDirection direction_ = ScanDirection::AtoB;
    lv_color_t color_{};
    // One continuous seven-corner polygon keeps the head, shoulders and
    // shortened tail completely sharp.  Drawing a separate rounded tail
    // caused the previous version's unwanted soft corners.
    lv_point_t arrowPoints_[7]{};
    ClickHandler clickHandler_ = nullptr;
    void* clickUserData_ = nullptr;
};

}  // namespace mg::p4
