#include "SoundToggleButton.h"

#include <stdlib.h>

#include "P4Button3D.h"
#include "P4Sound.h"

namespace mg::p4 {

namespace {
constexpr uint32_t kButtonColor = 0x294A5E;
}

void SoundToggleButton::create(lv_obj_t* parent,
                               lv_coord_t x,
                               lv_coord_t y,
                               lv_coord_t size) {
    button_ = lv_btn_create(parent);
    lv_obj_set_pos(button_, x, y);
    lv_obj_set_size(button_, size, size);
    applyP4Button3D(button_, P4ButtonTone::Sound, 8);
    lv_obj_add_event_cb(button_, clickCallback, LV_EVENT_CLICKED, this);

    canvas_ = lv_canvas_create(button_);
    lv_canvas_set_buffer(canvas_,
                         canvasBuffer_,
                         kIconWidth,
                         kIconHeight,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_center(canvas_);
    redrawIcon();
}

void SoundToggleButton::refresh() {
    redrawIcon();
}

void SoundToggleButton::clickCallback(lv_event_t* event) {
    auto* self = static_cast<SoundToggleButton*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        toggleP4SoundMuted();
        self->redrawIcon();
    }
}

void SoundToggleButton::redrawIcon() {
    if (canvas_ == nullptr) {
        return;
    }
    const lv_color_t background = lv_color_hex(kButtonColor);
    const lv_color_t white = lv_color_hex(0xF4F8FA);
    clear(background);

    // Speaker body and cone.
    fillRect(3, 11, 6, 7, white);
    for (lv_coord_t column = 0; column < 8; ++column) {
        const lv_coord_t halfHeight = static_cast<lv_coord_t>(3 + column / 2);
        fillRect(static_cast<lv_coord_t>(9 + column),
                 static_cast<lv_coord_t>(14 - halfHeight),
                 1,
                 static_cast<lv_coord_t>(halfHeight * 2 + 1),
                 white);
    }

    // Two sound-wave chevrons.
    drawLine(20, 10, 23, 14, 1, white);
    drawLine(23, 14, 20, 18, 1, white);
    drawLine(24, 7, 29, 14, 1, white);
    drawLine(29, 14, 24, 21, 1, white);

    if (p4SoundMuted()) {
        drawLine(2, 3, 30, 25, 3, lv_color_hex(0xF04444));
    }
    lv_obj_invalidate(canvas_);
}

void SoundToggleButton::clear(lv_color_t color) {
    fillRect(0, 0, kIconWidth, kIconHeight, color);
}

void SoundToggleButton::fillRect(lv_coord_t x,
                                 lv_coord_t y,
                                 lv_coord_t width,
                                 lv_coord_t height,
                                 lv_color_t color) {
    for (lv_coord_t row = 0; row < height; ++row) {
        for (lv_coord_t column = 0; column < width; ++column) {
            setPixel(static_cast<lv_coord_t>(x + column),
                     static_cast<lv_coord_t>(y + row),
                     color);
        }
    }
}

void SoundToggleButton::drawLine(lv_coord_t x0,
                                 lv_coord_t y0,
                                 lv_coord_t x1,
                                 lv_coord_t y1,
                                 lv_coord_t thickness,
                                 lv_color_t color) {
    int x = x0;
    int y = y0;
    const int dx = abs(static_cast<int>(x1 - x0));
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(static_cast<int>(y1 - y0));
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        const lv_coord_t radius = thickness / 2;
        fillRect(static_cast<lv_coord_t>(x - radius),
                 static_cast<lv_coord_t>(y - radius),
                 thickness,
                 thickness,
                 color);
        if (x == x1 && y == y1) {
            break;
        }
        const int doubled = 2 * error;
        if (doubled >= dy) {
            error += dy;
            x += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y += sy;
        }
    }
}

void SoundToggleButton::setPixel(lv_coord_t x,
                                 lv_coord_t y,
                                 lv_color_t color) {
    if (x < 0 || y < 0 || x >= kIconWidth || y >= kIconHeight) {
        return;
    }
    canvasBuffer_[static_cast<size_t>(y) * kIconWidth + x] = color;
}

}  // namespace mg::p4
