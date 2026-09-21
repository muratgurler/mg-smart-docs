#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_touch_gt911.h"
#include "gt911_touch.h"

#define CONFIG_LCD_HRES 1024
#define CONFIG_LCD_VRES 600

static const char *TAG = "GT911_PORT";

static esp_lcd_touch_handle_t tp = nullptr;
static esp_lcd_panel_io_handle_t tp_io_handle = nullptr;

static uint16_t touch_strength[1];
static uint8_t touch_cnt = 0;

gt911_touch::gt911_touch(int8_t sda_pin, int8_t scl_pin, int8_t rst_pin, int8_t int_pin)
{
    _sda = sda_pin;
    _scl = scl_pin;
    _rst = rst_pin;
    _int = int_pin;
}

bool gt911_touch::begin()
{
    _ready = false;
    tp = nullptr;
    tp_io_handle = nullptr;

    i2c_master_bus_handle_t i2c_handle = nullptr;
    esp_err_t result = i2c_master_get_bus_handle(I2C_NUM_1, &i2c_handle);
    if (result != ESP_OK || i2c_handle == nullptr) {
        ESP_LOGE(TAG, "I2C bus handle unavailable: %s (0x%x)",
                 esp_err_to_name(result), result);
        return false;
    }

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    result = esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle);
    if (result != ESP_OK || tp_io_handle == nullptr) {
        ESP_LOGE(TAG, "Touch panel IO initialization failed: %s (0x%x)",
                 esp_err_to_name(result), result);
        tp_io_handle = nullptr;
        return false;
    }

    // Use ordinary assignments to avoid the C++17 designated-initializer
    // field-order problem previously seen with the ESP-IDF DPI structure.
    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = CONFIG_LCD_HRES;
    tp_cfg.y_max = CONFIG_LCD_VRES;
    tp_cfg.rst_gpio_num = static_cast<gpio_num_t>(_rst);
    tp_cfg.int_gpio_num = static_cast<gpio_num_t>(_int);
    tp_cfg.levels.reset = 0;
    tp_cfg.levels.interrupt = 0;
    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 0;
    tp_cfg.flags.mirror_y = 0;

    ESP_LOGI(TAG, "Initialize touch controller gt911");
    esp_lcd_touch_handle_t candidate = nullptr;
    result = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &candidate);
    if (result != ESP_OK || candidate == nullptr) {
        // Touch is optional for boot.  Keep the display/UI alive so serial
        // controls and the scan demo remain usable while wiring is diagnosed.
        ESP_LOGE(TAG, "GT911 unavailable: %s (0x%x)",
                 esp_err_to_name(result), result);
        tp = nullptr;
        return false;
    }

    tp = candidate;
    _ready = true;
    return true;
}

bool gt911_touch::ready() const
{
    return _ready && tp != nullptr;
}

bool gt911_touch::getTouch(uint16_t *x, uint16_t *y)
{
    if (!ready() || x == nullptr || y == nullptr) {
        return false;
    }

    touch_cnt = 0;
    if (esp_lcd_touch_read_data(tp) != ESP_OK) {
        return false;
    }
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, x, y, touch_strength, &touch_cnt, 1);

    return touchpad_pressed;
}

void gt911_touch::set_rotation(uint8_t r)
{
    if (!ready()) {
        return;
    }

    switch(r){
    case 0:
        esp_lcd_touch_set_swap_xy(tp, false);   
        esp_lcd_touch_set_mirror_x(tp, false);
        esp_lcd_touch_set_mirror_y(tp, false);
        break;
    case 1:
        esp_lcd_touch_set_swap_xy(tp, false);
        esp_lcd_touch_set_mirror_x(tp, true);
        esp_lcd_touch_set_mirror_y(tp, true);
        break;
    case 2:
        esp_lcd_touch_set_swap_xy(tp, false);   
        esp_lcd_touch_set_mirror_x(tp, false);
        esp_lcd_touch_set_mirror_y(tp, false);
        break;
    case 3:
        esp_lcd_touch_set_swap_xy(tp, false);   
        esp_lcd_touch_set_mirror_x(tp, true);
        esp_lcd_touch_set_mirror_y(tp, true);
        break;
    }
}
