#include "MgResultTable.h"

namespace mg::p4 {

void MgResultTable::flat(lv_obj_t* object) {
    if (object == nullptr) return;
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_color_t MgResultTable::rowColor(MgResultStatus status) {
    switch (status) {
        case MgResultStatus::Pass:            return lv_color_hex(0x35D273);
        case MgResultStatus::Open:            return lv_color_hex(0xE9EEF2);
        case MgResultStatus::ShortCircuit:    return lv_color_hex(0xFFD34D);
        case MgResultStatus::WrongConnection: return lv_color_hex(0xF04444);
        case MgResultStatus::HighResistance:  return lv_color_hex(0xFF922E);
        case MgResultStatus::Warning:         return lv_color_hex(0xFFD34D);
        case MgResultStatus::Fail:            return lv_color_hex(0xF04444);
        case MgResultStatus::NotMeasured:     return lv_color_hex(0x35434E);
        case MgResultStatus::Info:
        default:                              return lv_color_hex(0x173345);
    }
}

lv_color_t MgResultTable::rowTextColor(MgResultStatus status) {
    switch (status) {
        case MgResultStatus::WrongConnection:
        case MgResultStatus::Fail:
        case MgResultStatus::NotMeasured:
        case MgResultStatus::Info:
            return lv_color_hex(0xFFFFFF);
        default:
            return lv_color_hex(0x101820);
    }
}

void MgResultTable::createHeaderSeparators(lv_obj_t* parent,
                                           lv_coord_t tableX,
                                           lv_coord_t tableY,
                                           lv_coord_t tableHeight,
                                           const lv_coord_t* widths) {
    if (parent == nullptr || widths == nullptr || columnCount_ < 2U) return;
    lv_coord_t x = tableX;
    for (uint16_t column = 0U; column + 1U < columnCount_; ++column) {
        x += widths[column];
        lv_obj_t* separator = lv_obj_create(parent);
        flat(separator);
        lv_obj_set_pos(separator, static_cast<lv_coord_t>(x - 1), tableY);
        lv_obj_set_size(separator, 2, tableHeight);
        lv_obj_set_style_border_width(separator, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(separator, lv_color_hex(0xA8D2E0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_clear_flag(separator, LV_OBJ_FLAG_CLICKABLE);
    }
}

void MgResultTable::create(lv_obj_t* parent,
                           lv_coord_t x,
                           lv_coord_t headerY,
                           lv_coord_t width,
                           lv_coord_t headerHeight,
                           lv_coord_t bodyY,
                           lv_coord_t bodyHeight,
                           uint16_t columnCount,
                           const lv_coord_t* columnWidths,
                           const lv_font_t* font) {
    if (parent == nullptr || columnWidths == nullptr || columnCount == 0U ||
        columnCount > kMaxColumns) return;

    columnCount_ = columnCount;
    resetStatuses();

    header_ = lv_table_create(parent);
    lv_obj_set_pos(header_, x, headerY);
    lv_obj_set_size(header_, width, headerHeight);
    lv_obj_clear_flag(header_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(header_, lv_color_hex(0x173345), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(header_, lv_color_hex(0x173345), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(header_, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_font(header_, font, LV_PART_ITEMS);
    lv_obj_set_style_text_color(header_, lv_color_hex(0xF1FAFF), LV_PART_ITEMS);
    lv_obj_set_style_text_align(header_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(header_, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(header_, lv_color_hex(0x527184), LV_PART_ITEMS);
    lv_obj_set_style_pad_all(header_, 5, LV_PART_ITEMS);

    body_ = lv_table_create(parent);
    lv_obj_set_pos(body_, x, bodyY);
    lv_obj_set_size(body_, width, bodyHeight);
    lv_obj_add_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(body_, lv_color_hex(0x0B1822), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(body_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(body_, lv_color_hex(0x0B1822), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(body_, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_font(body_, font, LV_PART_ITEMS);
    lv_obj_set_style_text_color(body_, lv_color_hex(0xE4EFF4), LV_PART_ITEMS);
    lv_obj_set_style_text_align(body_, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(body_, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(body_, lv_color_hex(0x243C49), LV_PART_ITEMS);
    lv_obj_set_style_pad_all(body_, 5, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(body_, lv_color_hex(0x527184), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(body_, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(body_, drawCallback, LV_EVENT_DRAW_PART_BEGIN, this);

    lv_table_set_col_cnt(header_, columnCount_);
    lv_table_set_row_cnt(header_, 1U);
    lv_table_set_col_cnt(body_, columnCount_);
    for (uint16_t column = 0U; column < columnCount_; ++column) {
        lv_table_set_col_width(header_, column, columnWidths[column]);
        lv_table_set_col_width(body_, column, columnWidths[column]);
    }

    // Explicit overlay bars make the 2 px separator invariant independent of
    // LVGL theme/cell-border rendering on the physical JC1060.
    createHeaderSeparators(parent, x, headerY, headerHeight, columnWidths);
}

void MgResultTable::setHeader(uint16_t column, const char* text) {
    if (header_ == nullptr || column >= columnCount_) return;
    lv_table_set_cell_value(header_, 0U, column, text != nullptr ? text : "");
}

void MgResultTable::setRowCount(uint16_t rows) {
    if (body_ == nullptr) return;
    if (rows > kMaxRows) rows = kMaxRows;
    lv_table_set_row_cnt(body_, rows);
}

void MgResultTable::setCell(uint16_t row, uint16_t column, const char* text) {
    if (body_ == nullptr || row >= kMaxRows || column >= columnCount_) return;
    lv_table_set_cell_value(body_, row, column, text != nullptr ? text : "");
}

void MgResultTable::setRowStatus(uint16_t row, MgResultStatus status) {
    if (row >= kMaxRows) return;
    rowStatuses_[row] = status;
}

void MgResultTable::resetStatuses(MgResultStatus status) {
    for (uint16_t row = 0U; row < kMaxRows; ++row) rowStatuses_[row] = status;
}

void MgResultTable::drawCallback(lv_event_t* event) {
    auto* self = static_cast<MgResultTable*>(lv_event_get_user_data(event));
    if (self == nullptr || self->body_ == nullptr) return;
    lv_obj_draw_part_dsc_t* dsc = lv_event_get_draw_part_dsc(event);
    if (dsc == nullptr || dsc->part != LV_PART_ITEMS || dsc->label_dsc == nullptr) return;

    // Physical target invariant: every value is centered beneath the center
    // of the corresponding fixed header cell.
    dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;

    if (dsc->rect_dsc == nullptr || self->columnCount_ == 0U) return;
    const uint32_t row = dsc->id / self->columnCount_;
    if (row >= kMaxRows) return;
    const MgResultStatus status = self->rowStatuses_[row];
    dsc->rect_dsc->bg_color = rowColor(status);
    dsc->rect_dsc->bg_opa = LV_OPA_COVER;
    dsc->label_dsc->color = rowTextColor(status);
}

}  // namespace mg::p4
