#pragma once

#include <lvgl.h>

namespace mg::p4 {

class SoundToggleButton {
public:
    void create(lv_obj_t* parent,
                lv_coord_t x,
                lv_coord_t y,
                lv_coord_t size = 42);
    void refresh();

private:
    static void clickCallback(lv_event_t* event);
    void redrawIcon();
    void clear(lv_color_t color);
    void fillRect(lv_coord_t x,
                  lv_coord_t y,
                  lv_coord_t width,
                  lv_coord_t height,
                  lv_color_t color);
    void drawLine(lv_coord_t x0,
                  lv_coord_t y0,
                  lv_coord_t x1,
                  lv_coord_t y1,
                  lv_coord_t thickness,
                  lv_color_t color);
    void setPixel(lv_coord_t x, lv_coord_t y, lv_color_t color);

    static constexpr lv_coord_t kIconWidth = 32;
    static constexpr lv_coord_t kIconHeight = 28;
    lv_obj_t* button_ = nullptr;
    lv_obj_t* canvas_ = nullptr;
    lv_color_t canvasBuffer_[kIconWidth * kIconHeight]{};
};

}  // namespace mg::p4
