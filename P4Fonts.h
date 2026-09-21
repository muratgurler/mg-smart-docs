#pragma once

#include <lvgl.h>

// Compact Latin fonts generated from DejaVu Sans.  Only ASCII plus the
// accented characters required by the seven UI languages are included.
// The large bold pin font contains only -0123456789EPSd so its flash cost
// stays bounded even though the active contact is shown at 84 px.
LV_FONT_DECLARE(mg_font_10)
LV_FONT_DECLARE(mg_font_14)
LV_FONT_DECLARE(mg_font_20)
LV_FONT_DECLARE(mg_font_pin_84)

namespace mg::p4 {

inline const lv_font_t* p4Font10() { return &mg_font_10; }
inline const lv_font_t* p4Font14() { return &mg_font_14; }
inline const lv_font_t* p4Font20() { return &mg_font_20; }
inline const lv_font_t* p4PinFont84() { return &mg_font_pin_84; }

}  // namespace mg::p4
