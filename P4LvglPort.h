#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "esp_lcd_mipi_dsi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ConfigP4.h"
#include "driver/i2c_master.h"
#include "src/lcd/jd9165_lcd.h"
#include "src/touch/gt911_touch.h"

namespace mg::p4 {

class P4LvglPort {
public:
    P4LvglPort();

    bool begin();
    void update();
    void setBacklight(bool enabled);

private:
    static void flushCallback(lv_disp_drv_t* display,
                              const lv_area_t* area,
                              lv_color_t* pixels);
    static void touchCallback(lv_indev_drv_t* input, lv_indev_data_t* data);
    static bool refreshDoneCallback(esp_lcd_panel_handle_t panel,
                                    esp_lcd_dpi_panel_event_data_t* event,
                                    void* userContext);

    static P4LvglPort* instance_;

    jd9165_lcd lcd_;
    gt911_touch touch_;
    esp_lcd_panel_handle_t panelHandle_ = nullptr;
    i2c_master_bus_handle_t touchBus_ = nullptr;
    lv_disp_draw_buf_t drawBuffer_{};
    lv_disp_drv_t displayDriver_{};
    lv_disp_t* display_ = nullptr;
    lv_indev_drv_t inputDriver_{};
    lv_color_t* frontBuffer_ = nullptr;
    lv_color_t* backBuffer_ = nullptr;
    SemaphoreHandle_t frameDoneSemaphore_ = nullptr;
    bool touchReady_ = false;
    bool ready_ = false;
};

}  // namespace mg::p4
