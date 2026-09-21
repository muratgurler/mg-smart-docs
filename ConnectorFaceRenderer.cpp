#include "ConnectorFaceRenderer.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdio.h>

#include "P4Fonts.h"

namespace mg::p4 {

namespace {

lv_obj_t* makeLabel(lv_obj_t* parent,
                    const char* text,
                    const lv_font_t* font,
                    lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    return label;
}

void setDotStyle(lv_obj_t* dot,
                 lv_color_t fill,
                 lv_color_t border,
                 uint8_t borderWidth) {
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, fill, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(dot, border, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, borderWidth, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
}

struct DSubGeometry {
    uint8_t rows;
    lv_coord_t flangeX;
    lv_coord_t flangeY;
    lv_coord_t flangeWidth;
    lv_coord_t flangeHeight;
    lv_coord_t shellX;
    lv_coord_t shellY;
    lv_coord_t shellTopWidth;
    lv_coord_t shellBottomWidth;
    lv_coord_t shellHeight;
};

DSubGeometry makeDSubGeometry(uint8_t contacts) {
    // Preserve the visible DA/DB/DC/DD/DE size progression, but compress the
    // ratio so even Sub-D9 stays large and readable on the fixed UI card.
    lv_coord_t flangeWidth = 168;
    if (contacts <= 9U) {
        flangeWidth = 132;
    } else if (contacts <= 15U) {
        flangeWidth = 142;
    } else if (contacts <= 25U) {
        flangeWidth = 154;
    } else if (contacts <= 37U) {
        flangeWidth = 166;
    }

    const uint8_t rows = contacts >= 44U ? 3U : 2U;
    const lv_coord_t flangeHeight = rows == 3U ? 104 : 94;
    const lv_coord_t flangeX =
        static_cast<lv_coord_t>((172 - flangeWidth) / 2);
    const lv_coord_t flangeY =
        static_cast<lv_coord_t>((136 - flangeHeight) / 2);
    const lv_coord_t shellTopWidth =
        static_cast<lv_coord_t>(flangeWidth - 28);
    const lv_coord_t shellBottomWidth =
        static_cast<lv_coord_t>(flangeWidth - 42);
    const lv_coord_t shellHeight =
        static_cast<lv_coord_t>(flangeHeight - 20);

    return DSubGeometry{
        rows,
        flangeX,
        flangeY,
        flangeWidth,
        flangeHeight,
        static_cast<lv_coord_t>((172 - shellTopWidth) / 2),
        static_cast<lv_coord_t>(flangeY + 10),
        shellTopWidth,
        shellBottomWidth,
        shellHeight,
    };
}

void dSubRowCounts(uint8_t contacts,
                   uint8_t rows,
                   uint8_t (&counts)[3]) {
    counts[0] = 0;
    counts[1] = 0;
    counts[2] = 0;
    if (rows == 2U) {
        counts[0] = static_cast<uint8_t>((contacts + 1U) / 2U);
        counts[1] = static_cast<uint8_t>(contacts / 2U);
        return;
    }

    // Standard-density DD-50 has matching outer rows. High-density DB-44
    // and DC-62 distribute their remainder into the first two rows.
    if (contacts == 50U) {
        counts[0] = 17U;
        counts[1] = 16U;
        counts[2] = 17U;
        return;
    }
    const uint8_t base = static_cast<uint8_t>(contacts / 3U);
    const uint8_t remainder = static_cast<uint8_t>(contacts % 3U);
    counts[0] = static_cast<uint8_t>(base + (remainder > 0U ? 1U : 0U));
    counts[1] = static_cast<uint8_t>(base + (remainder > 1U ? 1U : 0U));
    counts[2] = base;
}

}  // namespace

void ConnectorFaceRenderer::create(lv_obj_t* parent,
                                   lv_coord_t x,
                                   lv_coord_t y) {
    panel_ = lv_obj_create(parent);
    lv_obj_set_pos(panel_, x, y);
    lv_obj_set_size(panel_, 188, 194);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel_, lv_color_hex(0x101A23), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel_, lv_color_hex(0x294052), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel_, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(panel_, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel_, 0, LV_PART_MAIN);

    body_ = lv_canvas_create(panel_);
    lv_obj_set_pos(body_, 8, 10);
    const size_t bodyBytes = static_cast<size_t>(kBodyWidth) * kBodyHeight *
                             sizeof(lv_color_t);
    bodyBuffer_ = static_cast<lv_color_t*>(
        heap_caps_calloc(1,
                         bodyBytes,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (bodyBuffer_ != nullptr) {
        lv_canvas_set_buffer(body_,
                             bodyBuffer_,
                             kBodyWidth,
                             kBodyHeight,
                             LV_IMG_CF_TRUE_COLOR);
    } else {
        Serial.printf("[UI][ERR] connector canvas allocation failed: %u bytes\n",
                      static_cast<unsigned>(bodyBytes));
        Serial.flush();
        lv_obj_del(body_);
        body_ = lv_obj_create(panel_);
        lv_obj_set_pos(body_, 8, 10);
        lv_obj_set_size(body_, kBodyWidth, kBodyHeight);
        lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(body_,
                                  lv_color_hex(0x67717A),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(body_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(body_,
                                      lv_color_hex(0xD1DAE1),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_width(body_, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(body_, 18, LV_PART_MAIN);
    }

    peIndicator_ = lv_obj_create(panel_);
    lv_obj_set_pos(peIndicator_, 20, 158);
    lv_obj_set_size(peIndicator_, 58, 25);
    setDotStyle(peIndicator_,
                lv_color_hex(0x26333E),
                lv_color_hex(0x647481),
                1);
    lv_obj_t* peText = makeLabel(peIndicator_,
                                 "PE",
                                 p4Font14(),
                                 lv_color_hex(0xDCE6EC));
    lv_obj_center(peText);

    dsIndicator_ = lv_obj_create(panel_);
    lv_obj_set_pos(dsIndicator_, 110, 158);
    lv_obj_set_size(dsIndicator_, 58, 25);
    setDotStyle(dsIndicator_,
                lv_color_hex(0x26333E),
                lv_color_hex(0x647481),
                1);
    lv_obj_t* dsText = makeLabel(dsIndicator_,
                                 "dS",
                                 p4Font14(),
                                 lv_color_hex(0xDCE6EC));
    lv_obj_center(dsText);
}

void ConnectorFaceRenderer::setConnector(const ConnectorSpec& spec,
                                         bool includePe,
                                         bool includeDrainShield) {
    spec_ = spec;
    includePe_ = includePe;
    includeDrainShield_ = includeDrainShield;

    if (includePe_) {
        lv_obj_clear_flag(peIndicator_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(peIndicator_, LV_OBJ_FLAG_HIDDEN);
    }
    if (includeDrainShield_) {
        lv_obj_clear_flag(dsIndicator_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(dsIndicator_, LV_OBJ_FLAG_HIDDEN);
    }
    if (includePe_ && includeDrainShield_) {
        lv_obj_set_x(peIndicator_, 20);
        lv_obj_set_x(dsIndicator_, 110);
    } else if (includePe_) {
        lv_obj_set_x(peIndicator_, 65);
    } else if (includeDrainShield_) {
        lv_obj_set_x(dsIndicator_, 65);
    }
    char title[32] = {};
    snprintf(title,
             sizeof(title),
             "%s %s",
             spec.displayName,
             genderText(spec.gender));
    Serial.printf("[UI][CONN] %s title\n", title);
    Serial.flush();

    activeTestIndex_ = 0xFF;
    activeStatus_ = PointVisualStatus::Untested;
    activeIsSource_ = false;
    redrawBody();
    Serial.printf("[UI][CONN] %s pins ready\n", title);
    Serial.flush();
}

void ConnectorFaceRenderer::redrawBody() {
    if (bodyBuffer_ == nullptr || body_ == nullptr) {
        return;
    }

    fillRect(0, 0, kBodyWidth, kBodyHeight, lv_color_hex(0x101A23));

    if (spec_.kind == ConnectorKind::DSub) {
        drawDSubShell();
        drawDSubPins();
    } else {
        drawGenericPins();
    }
    lv_obj_invalidate(body_);
}

void ConnectorFaceRenderer::drawDSubShell() {
    const uint8_t contacts =
        spec_.contactCount > kMaxConnectorContacts ? kMaxConnectorContacts
                                                   : spec_.contactCount;
    const DSubGeometry geometry = makeDSubGeometry(contacts);

    // Reference-style Sub-D front face: rounded mounting plate, recessed
    // screw ears and a large tapered D shell. All dimensions are generated
    // from contact count rather than scaling a bitmap.
    fillRoundedRect(geometry.flangeX,
                    geometry.flangeY,
                    geometry.flangeWidth,
                    geometry.flangeHeight,
                    12,
                    lv_color_hex(0xD6DCE0));
    fillRoundedRect(static_cast<lv_coord_t>(geometry.flangeX + 3),
                    static_cast<lv_coord_t>(geometry.flangeY + 3),
                    static_cast<lv_coord_t>(geometry.flangeWidth - 6),
                    static_cast<lv_coord_t>(geometry.flangeHeight - 6),
                    10,
                    lv_color_hex(0x7A858D));

    fillRoundedTrapezoid(geometry.shellX,
                         geometry.shellY,
                         geometry.shellTopWidth,
                         geometry.shellBottomWidth,
                         geometry.shellHeight,
                         9,
                         lv_color_hex(0xE7EBED));
    fillRoundedTrapezoid(
        static_cast<lv_coord_t>(geometry.shellX + 4),
        static_cast<lv_coord_t>(geometry.shellY + 4),
        static_cast<lv_coord_t>(geometry.shellTopWidth - 8),
        static_cast<lv_coord_t>(geometry.shellBottomWidth - 8),
        static_cast<lv_coord_t>(geometry.shellHeight - 8),
        8,
        lv_color_hex(0x929CA3));
    fillRoundedTrapezoid(
        static_cast<lv_coord_t>(geometry.shellX + 8),
        static_cast<lv_coord_t>(geometry.shellY + 8),
        static_cast<lv_coord_t>(geometry.shellTopWidth - 16),
        static_cast<lv_coord_t>(geometry.shellBottomWidth - 16),
        static_cast<lv_coord_t>(geometry.shellHeight - 16),
        10,
        lv_color_hex(0x252B30));

    // Recessed jackscrew holes on both mounting ears.
    const lv_coord_t screwCenters[2] = {
        static_cast<lv_coord_t>(geometry.flangeX + 11),
        static_cast<lv_coord_t>(geometry.flangeX +
                                geometry.flangeWidth - 12),
    };
    const lv_coord_t screwY = static_cast<lv_coord_t>(
        geometry.flangeY + geometry.flangeHeight / 2);
    for (uint8_t index = 0; index < 2U; ++index) {
        const lv_coord_t centerX = screwCenters[index];
        fillCircle(centerX, screwY, 9, lv_color_hex(0xE4E8EA));
        fillCircle(centerX, screwY, 6, lv_color_hex(0x5B656C));
        fillCircle(centerX, screwY, 3, lv_color_hex(0x11161A));
    }
}

void ConnectorFaceRenderer::drawDSubPins() {
    const uint8_t contacts =
        spec_.contactCount > kMaxConnectorContacts ? kMaxConnectorContacts
                                                   : spec_.contactCount;
    const DSubGeometry geometry = makeDSubGeometry(contacts);
    const uint8_t rows = geometry.rows;
    uint8_t rowCounts[3] = {};
    dSubRowCounts(contacts, rows, rowCounts);
    uint8_t pin = 1;

    for (uint8_t row = 0; row < rows; ++row) {
        const uint8_t inRow = rowCounts[row];
        const lv_coord_t desiredPitch = contacts <= 9U  ? 17
                                      : contacts <= 15U ? 14
                                      : contacts <= 25U ? 9
                                      : contacts <= 37U ? 7
                                                        : 6;
        const lv_coord_t innerTopWidth =
            static_cast<lv_coord_t>(geometry.shellTopWidth - 16);
        const lv_coord_t innerBottomWidth =
            static_cast<lv_coord_t>(geometry.shellBottomWidth - 16);
        const lv_coord_t innerHeight =
            static_cast<lv_coord_t>(geometry.shellHeight - 16);
        const lv_coord_t rowCenterY = rows == 2U
            ? static_cast<lv_coord_t>(geometry.shellY + 8 +
                                      innerHeight * (row == 0U ? 35 : 68) / 100)
            : static_cast<lv_coord_t>(geometry.shellY + 8 +
                                      innerHeight * (27 + row * 23) / 100);
        const lv_coord_t relativeY = static_cast<lv_coord_t>(
            rowCenterY - (geometry.shellY + 8));
        const lv_coord_t availableWidth = static_cast<lv_coord_t>(
            innerTopWidth +
            (innerBottomWidth - innerTopWidth) * relativeY /
                (innerHeight > 1 ? innerHeight - 1 : 1));
        const lv_coord_t maximumPitch = inRow > 1U
            ? static_cast<lv_coord_t>((availableWidth - 4) / (inRow - 1U))
            : desiredPitch;
        const lv_coord_t pitch = desiredPitch < maximumPitch
                                     ? desiredPitch
                                     : maximumPitch;
        const lv_coord_t span = inRow > 1U
                                    ? static_cast<lv_coord_t>((inRow - 1U) * pitch)
                                    : 0;
        const lv_coord_t startX =
            static_cast<lv_coord_t>(kBodyWidth / 2 - span / 2);
        const lv_coord_t dotSize = contacts <= 15U ? 9
                                   : contacts <= 25U ? 7
                                   : contacts <= 37U ? 6
                                                     : 5;

        for (uint8_t column = 0; column < inRow && pin <= contacts;
             ++column, ++pin) {
            lv_coord_t centerX = static_cast<lv_coord_t>(
                startX + column * pitch);
            if (spec_.gender == ConnectorGender::Female) {
                centerX = static_cast<lv_coord_t>(kBodyWidth - centerX);
            }
            drawPin(static_cast<lv_coord_t>(centerX - dotSize / 2),
                    static_cast<lv_coord_t>(rowCenterY - dotSize / 2),
                    dotSize,
                    pin);
        }
    }
}

void ConnectorFaceRenderer::drawGenericPins() {
    const uint8_t contacts =
        spec_.contactCount > kMaxConnectorContacts ? kMaxConnectorContacts
                                                   : spec_.contactCount;
    if (contacts == 0) {
        return;
    }
    const uint8_t columns = contacts > 16U ? 16U : contacts;
    for (uint8_t pin = 1; pin <= contacts; ++pin) {
        const uint8_t column = static_cast<uint8_t>((pin - 1U) % columns);
        const uint8_t row = static_cast<uint8_t>((pin - 1U) / columns);
        drawPin(static_cast<lv_coord_t>(9 + column * 9),
                static_cast<lv_coord_t>(18 + row * 20),
                7,
                pin);
    }
}

void ConnectorFaceRenderer::drawPin(lv_coord_t x,
                                    lv_coord_t y,
                                    lv_coord_t size,
                                    uint8_t pin) {
    const bool active = pin == activeTestIndex_;
    const bool male = spec_.gender == ConnectorGender::Male;
    const lv_color_t normalFill = male ? lv_color_hex(0xD5A640)
                                       : lv_color_hex(0x151A1E);
    const lv_color_t normalBorder = male ? lv_color_hex(0xF8DB81)
                                         : lv_color_hex(0xD7E0E5);
    const lv_color_t fill = active ? pinColor(activeStatus_, activeIsSource_)
                                   : normalFill;
    const lv_color_t border = active ? lv_color_hex(0xFFFFFF) : normalBorder;
    const lv_coord_t radius = size / 2;
    const lv_coord_t centerX = static_cast<lv_coord_t>(x + radius);
    const lv_coord_t centerY = static_cast<lv_coord_t>(y + radius);
    fillCircle(centerX, centerY, radius, border);
    fillCircle(centerX,
               centerY,
               static_cast<lv_coord_t>(radius > 2 ? radius - 2 : radius - 1),
               fill);
}

void ConnectorFaceRenderer::fillRect(lv_coord_t x,
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

void ConnectorFaceRenderer::fillCircle(lv_coord_t centerX,
                                       lv_coord_t centerY,
                                       lv_coord_t radius,
                                       lv_color_t color) {
    const int radiusSquared = radius * radius;
    for (lv_coord_t y = static_cast<lv_coord_t>(-radius); y <= radius; ++y) {
        for (lv_coord_t x = static_cast<lv_coord_t>(-radius); x <= radius; ++x) {
            if (x * x + y * y <= radiusSquared) {
                setPixel(static_cast<lv_coord_t>(centerX + x),
                         static_cast<lv_coord_t>(centerY + y),
                         color);
            }
        }
    }
}

void ConnectorFaceRenderer::fillRoundedRect(lv_coord_t x,
                                             lv_coord_t y,
                                             lv_coord_t width,
                                             lv_coord_t height,
                                             lv_coord_t radius,
                                             lv_color_t color) {
    if (width <= 0 || height <= 0 || radius <= 0) {
        fillRect(x, y, width, height, color);
        return;
    }
    const lv_coord_t limitedRadius =
        radius > width / 2 ? width / 2
                           : (radius > height / 2 ? height / 2 : radius);
    fillRect(static_cast<lv_coord_t>(x + limitedRadius),
             y,
             static_cast<lv_coord_t>(width - 2 * limitedRadius),
             height,
             color);
    fillRect(x,
             static_cast<lv_coord_t>(y + limitedRadius),
             width,
             static_cast<lv_coord_t>(height - 2 * limitedRadius),
             color);
    fillCircle(static_cast<lv_coord_t>(x + limitedRadius),
               static_cast<lv_coord_t>(y + limitedRadius),
               limitedRadius,
               color);
    fillCircle(static_cast<lv_coord_t>(x + width - limitedRadius - 1),
               static_cast<lv_coord_t>(y + limitedRadius),
               limitedRadius,
               color);
    fillCircle(static_cast<lv_coord_t>(x + limitedRadius),
               static_cast<lv_coord_t>(y + height - limitedRadius - 1),
               limitedRadius,
               color);
    fillCircle(static_cast<lv_coord_t>(x + width - limitedRadius - 1),
               static_cast<lv_coord_t>(y + height - limitedRadius - 1),
               limitedRadius,
               color);
}

void ConnectorFaceRenderer::fillRoundedTrapezoid(lv_coord_t x,
                                                  lv_coord_t y,
                                                  lv_coord_t topWidth,
                                                  lv_coord_t bottomWidth,
                                                  lv_coord_t height,
                                                  lv_coord_t radius,
                                                  lv_color_t color) {
    if (height <= 0 || topWidth <= 0 || bottomWidth <= 0) {
        return;
    }
    const lv_coord_t limitedRadius =
        radius > height / 2 ? height / 2 : (radius < 0 ? 0 : radius);
    for (lv_coord_t row = 0; row < height; ++row) {
        const lv_coord_t width = static_cast<lv_coord_t>(
            topWidth + (bottomWidth - topWidth) * row /
                           (height > 1 ? height - 1 : 1));
        const lv_coord_t inset = static_cast<lv_coord_t>((topWidth - width) / 2);
        lv_coord_t cornerTrim = 0;
        if (limitedRadius > 0 && row < limitedRadius) {
            const lv_coord_t distance =
                static_cast<lv_coord_t>(limitedRadius - row);
            cornerTrim = static_cast<lv_coord_t>(
                (distance * distance + limitedRadius - 1) / limitedRadius);
        } else if (limitedRadius > 0 && row >= height - limitedRadius) {
            const lv_coord_t distance = static_cast<lv_coord_t>(
                row - (height - limitedRadius - 1));
            cornerTrim = static_cast<lv_coord_t>(
                (distance * distance + limitedRadius - 1) / limitedRadius);
        }
        const lv_coord_t roundedWidth =
            width > 2 * cornerTrim
                ? static_cast<lv_coord_t>(width - 2 * cornerTrim)
                : 1;
        fillRect(static_cast<lv_coord_t>(x + inset + cornerTrim),
                 static_cast<lv_coord_t>(y + row),
                 roundedWidth,
                 1,
                 color);
    }
}

void ConnectorFaceRenderer::setPixel(lv_coord_t x,
                                     lv_coord_t y,
                                     lv_color_t color) {
    if (bodyBuffer_ == nullptr || x < 0 || y < 0 || x >= kBodyWidth ||
        y >= kBodyHeight) {
        return;
    }
    bodyBuffer_[static_cast<size_t>(y) * kBodyWidth + x] = color;
}

void ConnectorFaceRenderer::setActiveTestIndex(uint8_t testIndex,
                                               PointVisualStatus status,
                                               bool isSource) {
    activeTestIndex_ = testIndex;
    activeStatus_ = status;
    activeIsSource_ = isSource;
    redrawBody();

    if (includePe_) {
        setDotStyle(peIndicator_,
                    lv_color_hex(0x26333E),
                    lv_color_hex(0x647481),
                    1);
    }
    if (includeDrainShield_) {
        setDotStyle(dsIndicator_,
                    lv_color_hex(0x26333E),
                    lv_color_hex(0x647481),
                    1);
    }

    const lv_color_t active = pinColor(status, isSource);
    if (includePe_ && testIndex == kPeTestIndex) {
        setDotStyle(peIndicator_, active, lv_color_hex(0xFFFFFF), 2);
    } else if (includeDrainShield_ && testIndex == kDrainShieldTestIndex) {
        setDotStyle(dsIndicator_, active, lv_color_hex(0xFFFFFF), 2);
    }
}

lv_color_t ConnectorFaceRenderer::pinColor(PointVisualStatus status,
                                           bool isSource) const {
    if (isSource || status == PointVisualStatus::Scanning) {
        return lv_color_hex(0x28BCE8);
    }
    switch (status) {
        case PointVisualStatus::Ok:
            return lv_color_hex(0x35D273);
        case PointVisualStatus::Open:
            return lv_color_hex(0xE9EEF2);
        case PointVisualStatus::ShortCircuit:
            return lv_color_hex(0xFFD34D);
        case PointVisualStatus::WrongConnection:
            return lv_color_hex(0xF04444);
        case PointVisualStatus::HighResistance:
            return lv_color_hex(0xFF922E);
        case PointVisualStatus::Untested:
        default:
            return lv_color_hex(0x596875);
    }
}

}  // namespace mg::p4
