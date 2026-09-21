#include "P4LvglPort.h"

namespace mg::p4 {

namespace {

void bootCheckpoint(const char* message) {
    Serial.println(message);
    Serial.flush();
}

}  // namespace

P4LvglPort* P4LvglPort::instance_ = nullptr;

P4LvglPort::P4LvglPort()
    : lcd_(kLcdResetPin),
      touch_(kTouchSdaPin,
             kTouchSclPin,
             kTouchResetPin,
             kTouchInterruptPin) {}

bool P4LvglPort::begin() {
    if (ready_) {
        return true;
    }
    instance_ = this;
    bootCheckpoint("[P4][01] display port begin");

    i2c_master_bus_config_t touchBusConfig = {};
    touchBusConfig.i2c_port = I2C_NUM_1;
    touchBusConfig.sda_io_num = static_cast<gpio_num_t>(kTouchSdaPin);
    touchBusConfig.scl_io_num = static_cast<gpio_num_t>(kTouchSclPin);
    touchBusConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    touchBusConfig.glitch_ignore_cnt = 7;
    touchBusConfig.intr_priority = 0;
    touchBusConfig.trans_queue_depth = 0;
    touchBusConfig.flags.enable_internal_pullup = true;

    const esp_err_t i2cResult =
        i2c_new_master_bus(&touchBusConfig, &touchBus_);
    if (i2cResult != ESP_OK) {
        Serial.printf("[P4] Touch I2C init failed: 0x%04X\n", i2cResult);
        return false;
    }
    bootCheckpoint("[P4][02] touch I2C ready");

    lcd_.begin();
    bootCheckpoint("[P4][03] JD9165 1024x600 panel ready");
    touchReady_ = touch_.begin();
    if (touchReady_) {
        bootCheckpoint("[P4][04] GT911 touch ready");
    } else {
        bootCheckpoint("[P4][WARN] GT911 unavailable; continuing without touch");
    }
    lcd_.get_handle(&panelHandle_);
    if (panelHandle_ == nullptr) {
        bootCheckpoint("[P4][ERR] LCD panel handle is null");
        return false;
    }

    lv_init();
    bootCheckpoint("[P4][05] LVGL initialized");

    static_assert(sizeof(lv_color_t) == 2U,
                  "Panel framebuffers require LVGL RGB565");
    void* panelFront = nullptr;
    void* panelBack = nullptr;
    const esp_err_t frameBufferResult = esp_lcd_dpi_panel_get_frame_buffer(
        panelHandle_,
        kFrameBufferCount,
        &panelFront,
        &panelBack);
    if (frameBufferResult != ESP_OK || panelFront == nullptr ||
        panelBack == nullptr) {
        Serial.printf("[P4] Native frame-buffer access failed: 0x%04X\n",
                      frameBufferResult);
        return false;
    }
    frontBuffer_ = static_cast<lv_color_t*>(panelFront);
    backBuffer_ = static_cast<lv_color_t*>(panelBack);
    frameDoneSemaphore_ = xSemaphoreCreateBinary();
    if (frameDoneSemaphore_ == nullptr) {
        Serial.println("[P4] Frame-sync semaphore allocation failed");
        return false;
    }
    Serial.printf("[P4][06] tear-free panel buffers ready: %u bytes x %u\n",
                  static_cast<unsigned>(kPanelFrameBytes),
                  static_cast<unsigned>(kFrameBufferCount));
    Serial.flush();

    lv_disp_draw_buf_init(&drawBuffer_,
                          frontBuffer_,
                          backBuffer_,
                          kPanelFramePixels);

    lv_disp_drv_init(&displayDriver_);
    displayDriver_.hor_res = kNativeDisplayWidth;
    displayDriver_.ver_res = kNativeDisplayHeight;
    displayDriver_.flush_cb = flushCallback;
    displayDriver_.draw_buf = &drawBuffer_;
    // Full refresh is deliberate: LVGL renders a complete coherent frame into
    // the inactive driver buffer, then the MIPI-DPI driver swaps it on VSYNC.
    displayDriver_.full_refresh = true;
    displayDriver_.sw_rotate = false;
    display_ = lv_disp_drv_register(&displayDriver_);
    if (display_ == nullptr) {
        Serial.println("[P4] LVGL display registration failed");
        return false;
    }
    bootCheckpoint("[P4][07] LVGL display registered");
    lv_disp_set_rotation(display_, LV_DISP_ROT_NONE);
    bootCheckpoint("[P4][08] native landscape coordinates enabled");

    if (touchReady_) {
        lv_indev_drv_init(&inputDriver_);
        inputDriver_.type = LV_INDEV_TYPE_POINTER;
        inputDriver_.read_cb = touchCallback;
        lv_indev_drv_register(&inputDriver_);
        bootCheckpoint("[P4][09] LVGL touch input registered");
    } else {
        bootCheckpoint("[P4][09] LVGL touch input skipped");
    }

    esp_lcd_dpi_panel_event_callbacks_t callbacks = {};
    callbacks.on_refresh_done = refreshDoneCallback;
    const esp_err_t callbackResult = esp_lcd_dpi_panel_register_event_callbacks(
        panelHandle_, &callbacks, this);
    if (callbackResult != ESP_OK) {
        Serial.printf("[P4] LCD transfer callback failed: 0x%04X\n",
                      callbackResult);
        return false;
    }
    bootCheckpoint("[P4][10] LCD refresh-done callback registered");

    ready_ = true;
    bootCheckpoint("[P4][11] display port ready");
    return true;
}

void P4LvglPort::update() {
    if (ready_) {
        lv_timer_handler();
    }
}

void P4LvglPort::setBacklight(bool enabled) {
    lcd_.example_bsp_set_lcd_backlight(enabled ? 1U : 0U);
}

void P4LvglPort::flushCallback(lv_disp_drv_t* display,
                               const lv_area_t* area,
                               lv_color_t* pixels) {
    if (instance_ == nullptr || display == nullptr || area == nullptr ||
        pixels == nullptr) {
        return;
    }

    // A full-refresh driver should issue one flush. Keep this guard so a
    // future LVGL change cannot swap an incomplete frame.
    if (!lv_disp_flush_is_last(display)) {
        lv_disp_flush_ready(display);
        return;
    }

    instance_->lcd_.lcd_draw_bitmap(0,
                                    0,
                                    kNativeDisplayWidth,
                                    kNativeDisplayHeight,
                                    &pixels->full);

    // Discard a stale periodic refresh event, then wait for the refresh that
    // presents the newly selected framebuffer. This is the same ordering used
    // by Espressif's tear-avoidance port and closes the pre-draw VSYNC race.
    xSemaphoreTake(instance_->frameDoneSemaphore_, 0);
    if (xSemaphoreTake(instance_->frameDoneSemaphore_,
                       pdMS_TO_TICKS(kFrameSyncTimeoutMs)) != pdTRUE) {
        Serial.println("[P4][WARN] frame refresh timeout; releasing LVGL");
        Serial.flush();
    }
    lv_disp_flush_ready(display);
}

void P4LvglPort::touchCallback(lv_indev_drv_t*, lv_indev_data_t* data) {
    if (instance_ == nullptr || !instance_->touchReady_) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint16_t x = 0;
    uint16_t y = 0;
    if (!instance_->touch_.getTouch(&x, &y)) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    data->state = LV_INDEV_STATE_PR;
    // JD9165, LVGL and GT911 all use native 1024x600 landscape coordinates.
    data->point.x = x < kNativeDisplayWidth ? x : (kNativeDisplayWidth - 1);
    data->point.y = y < kNativeDisplayHeight ? y : (kNativeDisplayHeight - 1);
}

bool P4LvglPort::refreshDoneCallback(esp_lcd_panel_handle_t,
                                     esp_lcd_dpi_panel_event_data_t*,
                                     void* userContext) {
    auto* self = static_cast<P4LvglPort*>(userContext);
    if (self == nullptr || self->frameDoneSemaphore_ == nullptr) {
        return false;
    }
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(self->frameDoneSemaphore_,
                          &higherPriorityTaskWoken);
    return higherPriorityTaskWoken == pdTRUE;
}

}  // namespace mg::p4
