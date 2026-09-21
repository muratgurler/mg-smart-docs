#include "P4LanguageVisuals.h"

namespace mg::p4 {

namespace {

constexpr lv_coord_t kFlagWidth = 44;
constexpr lv_coord_t kFlagHeight = 30;
constexpr lv_coord_t kFlagInnerWidth = 42;
constexpr lv_coord_t kFlagInnerHeight = 28;

struct RasterPoint {
    float x;
    float y;
};

// A filled ten-vertex star centred at the exact reference location. At this
// 42x28 raster size, filling the SVG's self-intersecting five-segment path
// directly collapses into a plus sign. These equivalent outline points keep
// all five tips readable on the physical LCD.
constexpr RasterPoint kReadableStar[10] = {
    {23.700F, 9.800F},
    {24.761F, 12.539F},
    {27.694F, 12.702F},
    {25.418F, 14.558F},
    {26.169F, 17.398F},
    {23.700F, 15.806F},
    {21.231F, 17.398F},
    {21.982F, 14.558F},
    {19.706F, 12.702F},
    {22.639F, 12.539F},
};

lv_color_t kTurkishFlagPixels[kFlagInnerWidth * kFlagInnerHeight];
bool turkishFlagPixelsReady = false;

lv_obj_t* iconPart(lv_obj_t* parent,
                   lv_coord_t x,
                   lv_coord_t y,
                   lv_coord_t width,
                   lv_coord_t height,
                   lv_color_t color,
                   bool round = false,
                   lv_opa_t opacity = LV_OPA_COVER) {
    lv_obj_t* part = lv_obj_create(parent);
    lv_obj_set_pos(part, x, y);
    lv_obj_set_size(part, width, height);
    lv_obj_clear_flag(part, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(part, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(part, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(part, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(part, round ? LV_RADIUS_CIRCLE : 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(part, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(part, opacity, LV_PART_MAIN);
    return part;
}

bool insideReadableStar(float x, float y) {
    int8_t windingNumber = 0;
    for (uint8_t current = 0; current < 10; ++current) {
        const RasterPoint& a = kReadableStar[current];
        const RasterPoint& b = kReadableStar[(current + 1U) % 10U];
        const float side =
            (b.x - a.x) * (y - a.y) - (x - a.x) * (b.y - a.y);
        if (a.y <= y) {
            if (b.y > y && side > 0.0F) {
                ++windingNumber;
            }
        } else if (b.y <= y && side < 0.0F) {
            --windingNumber;
        }
    }
    return windingNumber != 0;
}

void initializeTurkishFlagPixels() {
    if (turkishFlagPixelsReady) {
        return;
    }

    const lv_color_t red = lv_color_hex(0xE30A17);
    const lv_color_t white = lv_color_hex(0xFFFFFF);
    constexpr uint8_t samplesPerAxis = 4;
    constexpr uint8_t samplesPerPixel = samplesPerAxis * samplesPerAxis;
    for (lv_coord_t y = 0; y < kFlagInnerHeight; ++y) {
        for (lv_coord_t x = 0; x < kFlagInnerWidth; ++x) {
            uint8_t whiteSamples = 0;
            for (uint8_t sampleY = 0; sampleY < samplesPerAxis; ++sampleY) {
                for (uint8_t sampleX = 0; sampleX < samplesPerAxis; ++sampleX) {
                    const float localX = static_cast<float>(x) +
                        (static_cast<float>(sampleX) + 0.5F) /
                            static_cast<float>(samplesPerAxis);
                    const float localY = static_cast<float>(y) +
                        (static_cast<float>(sampleY) + 0.5F) /
                            static_cast<float>(samplesPerAxis);
                    const float referenceX =
                        localX * 512.0F /
                        static_cast<float>(kFlagInnerWidth);
                    const float referenceY =
                        localY * 356.18F /
                        static_cast<float>(kFlagInnerHeight);

                    const float outerDx =
                        (referenceX - 178.084F) / 89.047F;
                    const float outerDy =
                        (referenceY - 178.090F) / 89.047F;
                    const bool insideOuter =
                        outerDx * outerDx + outerDy * outerDy <= 1.0F;

                    const float innerDx =
                        (referenceX - 200.345F) / 71.237F;
                    const float innerDy =
                        (referenceY - 178.090F) / 71.237F;
                    const bool insideInner =
                        innerDx * innerDx + innerDy * innerDy <= 1.0F;

                    const bool crescent = insideOuter && !insideInner;
                    if (crescent || insideReadableStar(localX, localY)) {
                        ++whiteSamples;
                    }
                }
            }
            const lv_opa_t whiteOpacity = static_cast<lv_opa_t>(
                (static_cast<uint16_t>(whiteSamples) * 255U +
                 samplesPerPixel / 2U) /
                samplesPerPixel);
            kTurkishFlagPixels[y * kFlagInnerWidth + x] =
                lv_color_mix(white, red, whiteOpacity);
        }
    }
    turkishFlagPixelsReady = true;
}

void addTurkishReferenceArtwork(lv_obj_t* surface) {
    initializeTurkishFlagPixels();
    lv_obj_t* canvas = lv_canvas_create(surface);
    lv_canvas_set_buffer(canvas,
                         kTurkishFlagPixels,
                         kFlagInnerWidth,
                         kFlagInnerHeight,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(canvas, 0, 0);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace

lv_obj_t* createLanguageFlag(lv_obj_t* parent,
                             P4Language language,
                             lv_coord_t x,
                             lv_coord_t y) {
    lv_obj_t* flag = iconPart(parent,
                              x,
                              y,
                              kFlagWidth,
                              kFlagHeight,
                              lv_color_hex(0xFFFFFF));
    lv_obj_set_style_radius(flag, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(flag, lv_color_hex(0xD9E2E7), LV_PART_MAIN);
    lv_obj_set_style_border_width(flag, 1, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(flag, true, LV_PART_MAIN);

    lv_obj_t* surface = iconPart(flag,
                                 1,
                                 1,
                                 kFlagInnerWidth,
                                 kFlagInnerHeight,
                                 lv_color_hex(0xFFFFFF));

    switch (language) {
        case P4Language::Turkish:
            lv_obj_set_style_bg_color(surface,
                                      lv_color_hex(0xD51F2B),
                                      LV_PART_MAIN);
            addTurkishReferenceArtwork(surface);
            break;
        case P4Language::English:
            lv_obj_set_style_bg_color(surface,
                                      lv_color_hex(0x234A91),
                                      LV_PART_MAIN);
            iconPart(surface, 17, 0, 8, 28, lv_color_hex(0xFFFFFF));
            iconPart(surface, 0, 9, 42, 10, lv_color_hex(0xFFFFFF));
            iconPart(surface, 19, 0, 4, 28, lv_color_hex(0xD92332));
            iconPart(surface, 0, 11, 42, 6, lv_color_hex(0xD92332));
            break;
        case P4Language::Dutch:
            iconPart(surface, 0, 0, 42, 9, lv_color_hex(0xAE1C28));
            iconPart(surface, 0, 9, 42, 10, lv_color_hex(0xFFFFFF));
            iconPart(surface, 0, 19, 42, 9, lv_color_hex(0x21468B));
            break;
        case P4Language::German:
            iconPart(surface, 0, 0, 42, 9, lv_color_hex(0x111111));
            iconPart(surface, 0, 9, 42, 10, lv_color_hex(0xDD1F2D));
            iconPart(surface, 0, 19, 42, 9, lv_color_hex(0xFFCE00));
            break;
        case P4Language::French:
            iconPart(surface, 0, 0, 14, 28, lv_color_hex(0x1D3F8F));
            iconPart(surface, 14, 0, 14, 28, lv_color_hex(0xFFFFFF));
            iconPart(surface, 28, 0, 14, 28, lv_color_hex(0xED2939));
            break;
        case P4Language::Spanish:
            iconPart(surface, 0, 0, 42, 7, lv_color_hex(0xAA151B));
            iconPart(surface, 0, 7, 42, 14, lv_color_hex(0xF1BF00));
            iconPart(surface, 0, 21, 42, 7, lv_color_hex(0xAA151B));
            break;
        case P4Language::Polish:
            iconPart(surface, 0, 0, 42, 14, lv_color_hex(0xFFFFFF));
            iconPart(surface, 0, 14, 42, 14, lv_color_hex(0xDC143C));
            break;
        case P4Language::Count:
            break;
    }
    return flag;
}

lv_obj_t* createGlobeIcon(lv_obj_t* parent,
                          lv_coord_t x,
                          lv_coord_t y) {
    const lv_color_t lineColor = lv_color_hex(0xEAF6FF);
    lv_obj_t* globe = iconPart(parent,
                               x,
                               y,
                               28,
                               28,
                               lv_color_hex(0x000000),
                               true,
                               LV_OPA_TRANSP);
    lv_obj_set_style_border_color(globe, lineColor, LV_PART_MAIN);
    lv_obj_set_style_border_width(globe, 2, LV_PART_MAIN);

    lv_obj_t* meridian = iconPart(globe,
                                  7,
                                  1,
                                  14,
                                  26,
                                  lv_color_hex(0x000000),
                                  true,
                                  LV_OPA_TRANSP);
    lv_obj_set_style_border_color(meridian, lineColor, LV_PART_MAIN);
    lv_obj_set_style_border_width(meridian, 1, LV_PART_MAIN);
    iconPart(globe, 2, 8, 24, 1, lineColor);
    iconPart(globe, 2, 13, 24, 2, lineColor);
    iconPart(globe, 2, 20, 24, 1, lineColor);
    return globe;
}

}  // namespace mg::p4
