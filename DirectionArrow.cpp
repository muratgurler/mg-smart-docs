#include "DirectionArrow.h"

#include <esp_heap_caps.h>

namespace mg::p4 {

namespace {

lv_coord_t interpolateX(lv_coord_t x0,
                        lv_coord_t y0,
                        lv_coord_t x1,
                        lv_coord_t y1,
                        lv_coord_t y) {
    if (y1 == y0) {
        return x0;
    }
    return static_cast<lv_coord_t>(
        x0 + static_cast<int32_t>(x1 - x0) * (y - y0) / (y1 - y0));
}

void rasterizeSevenCornerArrow(lv_color_t* buffer,
                               lv_coord_t width,
                               lv_coord_t height,
                               const lv_point_t (&points)[7],
                               lv_color_t foreground,
                               lv_color_t background) {
    const size_t pixelCount = static_cast<size_t>(width) * height;
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
        buffer[pixel] = background;
    }

    const lv_coord_t topY = points[0].y;
    const lv_coord_t middleY = points[1].y;
    const lv_coord_t bottomY = points[2].y;
    const lv_coord_t lowerShoulderY = points[3].y;
    const lv_coord_t tailX = points[4].x;
    const lv_coord_t upperShoulderY = points[5].y;
    const lv_coord_t baseX = points[0].x;
    const lv_coord_t tipX = points[1].x;

    for (lv_coord_t y = topY; y <= bottomY; ++y) {
        const lv_coord_t slopedX = y <= middleY
                                       ? interpolateX(baseX,
                                                      topY,
                                                      tipX,
                                                      middleY,
                                                      y)
                                       : interpolateX(tipX,
                                                      middleY,
                                                      baseX,
                                                      bottomY,
                                                      y);
        const bool insideTail =
            y >= upperShoulderY && y <= lowerShoulderY;
        const lv_coord_t straightX = insideTail ? tailX : baseX;
        lv_coord_t firstX = straightX < slopedX ? straightX : slopedX;
        lv_coord_t lastX = straightX > slopedX ? straightX : slopedX;

        if (y < 0 || y >= height || lastX < 0 || firstX >= width) {
            continue;
        }
        if (firstX < 0) firstX = 0;
        if (lastX >= width) lastX = static_cast<lv_coord_t>(width - 1);

        lv_color_t* row = buffer + static_cast<size_t>(y) * width;
        for (lv_coord_t x = firstX; x <= lastX; ++x) {
            row[x] = foreground;
        }
    }
}

}  // namespace

void DirectionArrow::create(lv_obj_t* parent,
                            lv_coord_t x,
                            lv_coord_t y,
                            ClickHandler clickHandler,
                            void* clickUserData) {
    clickHandler_ = clickHandler;
    clickUserData_ = clickUserData;
    container_ = lv_obj_create(parent);
    lv_obj_set_pos(container_, x, y);
    lv_obj_set_size(container_, kWidth, kHeight);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(container_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container_, 0, LV_PART_MAIN);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(container_, eventCallback, LV_EVENT_CLICKED, this);

    canvas_ = lv_canvas_create(container_);
    lv_obj_set_pos(canvas_, 0, 0);
    lv_obj_set_size(canvas_, kWidth, kHeight);
    lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
    const size_t bufferBytes =
        static_cast<size_t>(kWidth) * kHeight * sizeof(lv_color_t);
    canvasBuffer_ = static_cast<lv_color_t*>(
        heap_caps_calloc(1,
                         bufferBytes,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (canvasBuffer_ != nullptr) {
        lv_canvas_set_buffer(canvas_,
                             canvasBuffer_,
                             kWidth,
                             kHeight,
                             LV_IMG_CF_TRUE_COLOR);
    }

    color_ = lv_color_hex(0x24D07A);
    redraw();
}

void DirectionArrow::eventCallback(lv_event_t* event) {
    auto* self = static_cast<DirectionArrow*>(lv_event_get_user_data(event));
    if (self != nullptr && self->clickHandler_ != nullptr) {
        self->clickHandler_(self->clickUserData_);
    }
}

void DirectionArrow::setDirection(ScanDirection direction, lv_color_t color) {
    if (container_ == nullptr || canvas_ == nullptr || canvasBuffer_ == nullptr) {
        return;
    }
    direction_ = direction;
    color_ = color;
    redraw();
}

void DirectionArrow::redraw() {
    if (canvas_ == nullptr || canvasBuffer_ == nullptr) {
        return;
    }

    // These coordinates are identical to the approved 1024x600 preview.
    // The shape is rasterized directly into the canvas buffer because the
    // LVGL polygon backend rendered a different five-corner silhouette on
    // the physical JC1060 target.
    if (direction_ == ScanDirection::AtoB) {
        arrowPoints_[0] = {142, 8};
        arrowPoints_[1] = {226, 46};
        arrowPoints_[2] = {142, 84};
        arrowPoints_[3] = {142, 61};
        arrowPoints_[4] = {72, 61};
        arrowPoints_[5] = {72, 31};
        arrowPoints_[6] = {142, 31};
    } else {
        arrowPoints_[0] = {94, 8};
        arrowPoints_[1] = {10, 46};
        arrowPoints_[2] = {94, 84};
        arrowPoints_[3] = {94, 61};
        arrowPoints_[4] = {164, 61};
        arrowPoints_[5] = {164, 31};
        arrowPoints_[6] = {94, 31};
    }

    rasterizeSevenCornerArrow(canvasBuffer_,
                              kWidth,
                              kHeight,
                              arrowPoints_,
                              color_,
                              lv_color_hex(0x0B1118));
    lv_obj_invalidate(canvas_);
}

}  // namespace mg::p4
