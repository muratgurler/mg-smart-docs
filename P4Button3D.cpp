#include "P4Button3D.h"

namespace mg::p4 {

namespace {

struct ToneStyles {
    lv_style_t normal{};
    lv_style_t pressed{};
};

lv_style_t baseStyle{};
lv_style_t pressedGeometryStyle{};
ToneStyles neutralStyles{};
ToneStyles activeStyles{};
ToneStyles soundStyles{};
bool stylesInitialized = false;

void initializeTone(ToneStyles& styles,
                    uint32_t top,
                    uint32_t bottom,
                    uint32_t border,
                    uint32_t pressedTop,
                    uint32_t pressedBottom,
                    uint32_t pressedBorder) {
    lv_style_init(&styles.normal);
    lv_style_set_bg_opa(&styles.normal, LV_OPA_COVER);
    lv_style_set_bg_color(&styles.normal, lv_color_hex(top));
    lv_style_set_bg_grad_color(&styles.normal, lv_color_hex(bottom));
    lv_style_set_bg_grad_dir(&styles.normal, LV_GRAD_DIR_VER);
    lv_style_set_border_color(&styles.normal, lv_color_hex(border));

    lv_style_init(&styles.pressed);
    lv_style_set_bg_opa(&styles.pressed, LV_OPA_COVER);
    lv_style_set_bg_color(&styles.pressed, lv_color_hex(pressedTop));
    lv_style_set_bg_grad_color(&styles.pressed,
                               lv_color_hex(pressedBottom));
    lv_style_set_bg_grad_dir(&styles.pressed, LV_GRAD_DIR_VER);
    lv_style_set_border_color(&styles.pressed,
                              lv_color_hex(pressedBorder));
}

void initializeStyles() {
    if (stylesInitialized) {
        return;
    }
    stylesInitialized = true;

    lv_style_init(&baseStyle);
    lv_style_set_border_width(&baseStyle, 2);
    lv_style_set_border_opa(&baseStyle, LV_OPA_COVER);
    lv_style_set_shadow_color(&baseStyle, lv_color_hex(0x000000));
    lv_style_set_shadow_width(&baseStyle, 5);
    lv_style_set_shadow_ofs_y(&baseStyle, 3);
    lv_style_set_shadow_opa(&baseStyle, static_cast<lv_opa_t>(110));
    lv_style_set_pad_all(&baseStyle, 0);

    lv_style_init(&pressedGeometryStyle);
    lv_style_set_translate_y(&pressedGeometryStyle, 2);
    lv_style_set_shadow_width(&pressedGeometryStyle, 1);
    lv_style_set_shadow_ofs_y(&pressedGeometryStyle, 1);
    lv_style_set_shadow_opa(&pressedGeometryStyle,
                            static_cast<lv_opa_t>(60));

    initializeTone(neutralStyles,
                   0x68747B,
                   0x3C464C,
                   0x929EA5,
                   0x354047,
                   0x566169,
                   0xAAB4BA);
    initializeTone(activeStyles,
                   0x27AA67,
                   0x0E6738,
                   0x76E2AA,
                   0x0A5A30,
                   0x1E8C53,
                   0x8BEBB8);
    // The speaker canvas has an opaque background. Keeping both gradient
    // endpoints equal prevents a visible rectangle behind its icon.
    initializeTone(soundStyles,
                   0x294A5E,
                   0x294A5E,
                   0x7790A0,
                   0x203A4A,
                   0x203A4A,
                   0x91A8B6);
}

}  // namespace

void applyP4Button3D(lv_obj_t* button,
                     P4ButtonTone tone,
                     lv_coord_t radius) {
    if (button == nullptr) {
        return;
    }
    initializeStyles();
    lv_obj_set_style_radius(button, radius, LV_PART_MAIN);
    lv_obj_add_style(button, &baseStyle, LV_PART_MAIN);
    lv_obj_add_style(button,
                     &pressedGeometryStyle,
                     LV_PART_MAIN | LV_STATE_PRESSED);
    if (tone == P4ButtonTone::Sound) {
        lv_obj_add_style(button, &soundStyles.normal, LV_PART_MAIN);
        lv_obj_add_style(button,
                         &soundStyles.pressed,
                         LV_PART_MAIN | LV_STATE_PRESSED);
        return;
    }

    // Neutral and active appearances stay attached for the object's lifetime.
    // LV_STATE_CHECKED selects green without removing/adding styles on every
    // scan refresh, avoiding heap churn during long production runs.
    lv_obj_add_style(button, &neutralStyles.normal, LV_PART_MAIN);
    lv_obj_add_style(button,
                     &neutralStyles.pressed,
                     LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(button,
                     &activeStyles.normal,
                     LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_style(button,
                     &activeStyles.pressed,
                     LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    setP4Button3DTone(button, tone);
}

void setP4Button3DTone(lv_obj_t* button, P4ButtonTone tone) {
    if (button == nullptr) {
        return;
    }
    if (tone == P4ButtonTone::Active) {
        lv_obj_add_state(button, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(button, LV_STATE_CHECKED);
    }
}

}  // namespace mg::p4
