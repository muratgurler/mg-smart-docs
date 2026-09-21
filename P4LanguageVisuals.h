#pragma once

#include <lvgl.h>

#include "P4Localization.h"

namespace mg::p4 {

// Draws a fixed 44x30 country flag. The Turkish crescent/star is rasterized
// from the geometry of the approved reference SVG.
lv_obj_t* createLanguageFlag(lv_obj_t* parent,
                             P4Language language,
                             lv_coord_t x,
                             lv_coord_t y);

// Draws a fixed 28x28 globe icon without relying on an emoji/font glyph.
lv_obj_t* createGlobeIcon(lv_obj_t* parent,
                          lv_coord_t x,
                          lv_coord_t y);

}  // namespace mg::p4
