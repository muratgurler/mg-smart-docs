#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

namespace mg::p4 {

enum class MgResultStatus : uint8_t {
    Info = 0,
    Pass,
    Open,
    ShortCircuit,
    WrongConnection,
    HighResistance,
    Warning,
    Fail,
    NotMeasured,
};

// Shared result-table renderer used by every detailed MG test-result page.
// Frozen visual contract:
// - fixed header
// - body scrolls vertically only
// - header/body use identical column widths
// - every cell is centered under its header at style + draw time
// - real 2 px vertical header separators
// - natural row order; status changes color only
class MgResultTable {
public:
    static constexpr uint16_t kMaxRows = 256U;
    static constexpr uint16_t kMaxColumns = 8U;

    void create(lv_obj_t* parent,
                lv_coord_t x,
                lv_coord_t headerY,
                lv_coord_t width,
                lv_coord_t headerHeight,
                lv_coord_t bodyY,
                lv_coord_t bodyHeight,
                uint16_t columnCount,
                const lv_coord_t* columnWidths,
                const lv_font_t* font);

    void setHeader(uint16_t column, const char* text);
    void setRowCount(uint16_t rows);
    void setCell(uint16_t row, uint16_t column, const char* text);
    void setRowStatus(uint16_t row, MgResultStatus status);
    void resetStatuses(MgResultStatus status = MgResultStatus::Info);

    lv_obj_t* header() const { return header_; }
    lv_obj_t* body() const { return body_; }
    uint16_t columnCount() const { return columnCount_; }

    static lv_color_t rowColor(MgResultStatus status);
    static lv_color_t rowTextColor(MgResultStatus status);

private:
    static void drawCallback(lv_event_t* event);
    static void flat(lv_obj_t* object);
    void createHeaderSeparators(lv_obj_t* parent,
                                lv_coord_t tableX,
                                lv_coord_t tableY,
                                lv_coord_t tableHeight,
                                const lv_coord_t* widths);

    lv_obj_t* header_ = nullptr;
    lv_obj_t* body_ = nullptr;
    uint16_t columnCount_ = 0U;
    MgResultStatus rowStatuses_[kMaxRows]{};
};

}  // namespace mg::p4
