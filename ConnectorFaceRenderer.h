#pragma once

#include <lvgl.h>

#include "CableProfile.h"
#include "ScanSession.h"

namespace mg::p4 {

class ConnectorFaceRenderer {
public:
    void create(lv_obj_t* parent,
                lv_coord_t x,
                lv_coord_t y);
    void setConnector(const ConnectorSpec& spec,
                      bool includePe,
                      bool includeDrainShield);
    void setActiveTestIndex(uint8_t testIndex,
                            PointVisualStatus status,
                            bool isSource);

private:
    void redrawBody();
    void drawDSubShell();
    void drawDSubPins();
    void drawGenericPins();
    void drawPin(lv_coord_t x, lv_coord_t y, lv_coord_t size, uint8_t pin);
    void fillRect(lv_coord_t x,
                  lv_coord_t y,
                  lv_coord_t width,
                  lv_coord_t height,
                  lv_color_t color);
    void fillCircle(lv_coord_t centerX,
                    lv_coord_t centerY,
                    lv_coord_t radius,
                    lv_color_t color);
    void fillRoundedRect(lv_coord_t x,
                         lv_coord_t y,
                         lv_coord_t width,
                         lv_coord_t height,
                         lv_coord_t radius,
                         lv_color_t color);
    void fillRoundedTrapezoid(lv_coord_t x,
                              lv_coord_t y,
                              lv_coord_t topWidth,
                              lv_coord_t bottomWidth,
                              lv_coord_t height,
                              lv_coord_t radius,
                              lv_color_t color);
    void setPixel(lv_coord_t x, lv_coord_t y, lv_color_t color);
    lv_color_t pinColor(PointVisualStatus status, bool isSource) const;

    static constexpr uint8_t kMaxConnectorContacts = 62;
    static constexpr lv_coord_t kBodyWidth = 172;
    static constexpr lv_coord_t kBodyHeight = 136;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* body_ = nullptr;
    lv_color_t* bodyBuffer_ = nullptr;
    lv_obj_t* peIndicator_ = nullptr;
    lv_obj_t* dsIndicator_ = nullptr;
    ConnectorSpec spec_{ConnectorKind::Custom,
                        ConnectorGender::Neutral,
                        0,
                        "--",
                        nullptr};
    uint8_t activeTestIndex_ = 0xFF;
    PointVisualStatus activeStatus_ = PointVisualStatus::Untested;
    bool activeIsSource_ = false;
    bool includePe_ = true;
    bool includeDrainShield_ = true;
};

}  // namespace mg::p4
