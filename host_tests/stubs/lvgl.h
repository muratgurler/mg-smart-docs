#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int16_t lv_coord_t;
typedef uint32_t lv_color_t;
typedef uint8_t lv_opa_t;
struct lv_obj_t {};
struct lv_event_t {};
struct lv_font_t {};
struct lv_draw_ctx_t {};
struct lv_point_t { lv_coord_t x = 0; lv_coord_t y = 0; };
struct lv_area_t { lv_coord_t x1 = 0; lv_coord_t y1 = 0; lv_coord_t x2 = 0; lv_coord_t y2 = 0; };
struct lv_draw_line_dsc_t { int width = 0; lv_color_t color = 0; int opa = 0; };
struct lv_draw_rect_dsc_t { lv_color_t bg_color = 0; int bg_opa = 0; int radius = 0; int border_width = 0; };
struct lv_draw_label_dsc_t { int align = 0; lv_color_t color = 0; };
struct lv_obj_draw_part_dsc_t { int part = 0; uint32_t id = 0; lv_draw_rect_dsc_t* rect_dsc = nullptr; lv_draw_label_dsc_t* label_dsc = nullptr; };
typedef void (*lv_event_cb_t)(lv_event_t*);
#define LV_FONT_DECLARE(x) extern lv_font_t x;
#define LV_PART_MAIN 0
#define LV_PART_INDICATOR 1
#define LV_PART_ITEMS 2
#define LV_PART_SCROLLBAR 3
#define LV_OBJ_FLAG_SCROLLABLE 1
#define LV_OBJ_FLAG_CLICKABLE 2
#define LV_OBJ_FLAG_PRESS_LOCK 4
#define LV_OBJ_FLAG_HIDDEN 8
#define LV_OPA_COVER 255
#define LV_OPA_TRANSP 0
#define LV_OPA_70 178
#define LV_OPA_50 128
#define LV_ALIGN_TOP_MID 0
#define LV_ALIGN_BOTTOM_MID 1
#define LV_ALIGN_TOP_RIGHT 2
#define LV_ALIGN_BOTTOM_RIGHT 3
#define LV_TEXT_ALIGN_CENTER 0
#define LV_TEXT_ALIGN_LEFT 1
#define LV_TEXT_ALIGN_RIGHT 2
#define LV_LABEL_LONG_WRAP 0
#define LV_LABEL_LONG_DOT 1
#define LV_EVENT_CLICKED 0
#define LV_EVENT_DRAW_PART_BEGIN 1
#define LV_EVENT_DRAW_MAIN 3
#define LV_STATE_DISABLED 1
#define LV_ANIM_OFF 0
#define LV_FLEX_FLOW_ROW 0
#define LV_FLEX_ALIGN_CENTER 0
#define LV_DIR_VER 1
#define LV_SCROLLBAR_MODE_AUTO 0
#define LV_RADIUS_CIRCLE 999
inline lv_color_t lv_color_hex(uint32_t x){return x;}
inline lv_obj_t* lv_obj_create(lv_obj_t*){return nullptr;}
inline lv_obj_t* lv_btn_create(lv_obj_t*){return nullptr;}
inline lv_obj_t* lv_label_create(lv_obj_t*){return nullptr;}
inline lv_obj_t* lv_bar_create(lv_obj_t*){return nullptr;}
inline lv_obj_t* lv_table_create(lv_obj_t*){return nullptr;}
inline void lv_obj_clear_flag(lv_obj_t*, int){}
inline void lv_obj_add_flag(lv_obj_t*, int){}
inline void lv_obj_set_style_pad_all(lv_obj_t*, int, int){}
inline void lv_obj_set_style_pad_column(lv_obj_t*, int, int){}
inline void lv_label_set_text(lv_obj_t*, const char*){}
inline void lv_label_set_long_mode(lv_obj_t*, int){}
inline void lv_obj_set_style_text_font(lv_obj_t*, const lv_font_t*, int){}
inline void lv_obj_set_style_text_color(lv_obj_t*, lv_color_t, int){}
inline void lv_obj_set_style_text_align(lv_obj_t*, int, int){}
inline void lv_obj_set_style_bg_color(lv_obj_t*, lv_color_t, int){}
inline void lv_obj_set_style_bg_opa(lv_obj_t*, int, int){}
inline void lv_obj_set_style_border_width(lv_obj_t*, int, int){}
inline void lv_obj_set_style_border_color(lv_obj_t*, lv_color_t, int){}
inline void lv_obj_set_style_radius(lv_obj_t*, int, int){}
inline void lv_obj_set_pos(lv_obj_t*, lv_coord_t, lv_coord_t){}
inline void lv_obj_set_size(lv_obj_t*, lv_coord_t, lv_coord_t){}
inline void lv_obj_set_width(lv_obj_t*, lv_coord_t){}
inline void lv_obj_set_height(lv_obj_t*, lv_coord_t){}
inline lv_coord_t lv_obj_get_width(lv_obj_t*){return 900;}
inline lv_coord_t lv_obj_get_height(lv_obj_t*){return 404;}
inline void lv_obj_get_coords(lv_obj_t*, lv_area_t*){}
inline void lv_obj_align(lv_obj_t*, int, lv_coord_t, lv_coord_t){}
inline void lv_obj_center(lv_obj_t*){}
inline void lv_obj_clean(lv_obj_t*){}
inline void lv_obj_del(lv_obj_t*){}
inline void lv_scr_load(lv_obj_t*){}
inline lv_obj_t* lv_scr_act(){return nullptr;}
inline void lv_obj_update_layout(lv_obj_t*){}
inline void lv_obj_invalidate(lv_obj_t*){}
inline void lv_draw_line_dsc_init(lv_draw_line_dsc_t*){}
inline void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t*){}
inline void lv_draw_line(lv_draw_ctx_t*, const lv_draw_line_dsc_t*, const lv_point_t*, const lv_point_t*){}
inline void lv_draw_rect(lv_draw_ctx_t*, const lv_draw_rect_dsc_t*, const lv_area_t*){}
inline void lv_obj_add_event_cb(lv_obj_t*, lv_event_cb_t, int, void*){}
inline bool lv_obj_remove_event_cb(lv_obj_t*, lv_event_cb_t){return true;}
inline void* lv_event_get_user_data(lv_event_t*){return nullptr;}
inline lv_draw_ctx_t* lv_event_get_draw_ctx(lv_event_t*){return nullptr;}
inline lv_obj_t* lv_event_get_target(lv_event_t*){return nullptr;}
inline lv_obj_draw_part_dsc_t* lv_event_get_draw_part_dsc(lv_event_t*){return nullptr;}
inline void lv_obj_add_state(lv_obj_t*, int){}
inline void lv_obj_clear_state(lv_obj_t*, int){}
inline void lv_obj_set_ext_click_area(lv_obj_t*, lv_coord_t){}
inline void lv_obj_set_flex_flow(lv_obj_t*, int){}
inline void lv_obj_set_flex_align(lv_obj_t*, int,int,int){}
inline void lv_obj_set_scroll_dir(lv_obj_t*, int){}
inline void lv_obj_set_scrollbar_mode(lv_obj_t*, int){}
inline void lv_table_set_col_cnt(lv_obj_t*, uint16_t){}
inline void lv_table_set_row_cnt(lv_obj_t*, uint16_t){}
inline void lv_table_set_col_width(lv_obj_t*, uint16_t, lv_coord_t){}
inline uint16_t lv_table_get_col_cnt(lv_obj_t*){return 7;}
inline void lv_table_set_cell_value(lv_obj_t*, uint16_t, uint16_t, const char*){}
inline void lv_bar_set_range(lv_obj_t*, int, int){}
inline void lv_bar_set_value(lv_obj_t*, int, int){}
inline lv_obj_t* lv_obj_get_child(lv_obj_t*, int){return nullptr;}

#define LV_EVENT_VALUE_CHANGED 2
#define LV_STATE_FOCUSED 2
#define LV_BTNMATRIX_BTN_NONE 0xFFFFU
inline lv_obj_t* lv_textarea_create(lv_obj_t*){return nullptr;}
inline lv_obj_t* lv_btnmatrix_create(lv_obj_t*){return nullptr;}
inline void lv_textarea_set_one_line(lv_obj_t*, bool){}
inline void lv_textarea_set_password_mode(lv_obj_t*, bool){}
inline void lv_textarea_set_max_length(lv_obj_t*, uint32_t){}
inline void lv_textarea_set_placeholder_text(lv_obj_t*, const char*){}
inline void lv_textarea_set_text(lv_obj_t*, const char*){}
inline const char* lv_textarea_get_text(lv_obj_t*){return "";}
inline void lv_textarea_del_char(lv_obj_t*){}
inline void lv_textarea_cursor_left(lv_obj_t*){}
inline void lv_textarea_cursor_right(lv_obj_t*){}
inline void lv_textarea_add_text(lv_obj_t*, const char*){}
inline void lv_textarea_add_char(lv_obj_t*, uint32_t){}
inline void lv_btnmatrix_set_map(lv_obj_t*, const char* const*){}
inline uint16_t lv_btnmatrix_get_selected_btn(lv_obj_t*){return LV_BTNMATRIX_BTN_NONE;}
inline const char* lv_btnmatrix_get_btn_text(lv_obj_t*, uint16_t){return "";}
inline int lv_event_get_code(lv_event_t*){return 0;}
