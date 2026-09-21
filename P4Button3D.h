#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace mg::p4 {

enum class P4ButtonTone : uint8_t {
    Neutral,
    Active,
    Sound,
};

// Shared LVGL styles keep the 3D appearance inexpensive: every button stores
// only references to the same bevel, shadow and colour styles.
void applyP4Button3D(lv_obj_t* button,
                     P4ButtonTone tone = P4ButtonTone::Neutral,
                     lv_coord_t radius = 8);
void setP4Button3DTone(lv_obj_t* button, P4ButtonTone tone);

}  // namespace mg::p4
