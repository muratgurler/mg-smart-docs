get_filename_component(MG_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

file(GLOB MG_ROOT_CPP "${MG_ROOT}/*.cpp")
file(GLOB_RECURSE MG_DRIVER_CPP "${MG_ROOT}/src/*.cpp")
file(GLOB_RECURSE MG_DRIVER_C "${MG_ROOT}/src/*.c")

idf_component_register(
    SRCS
        "${CMAKE_CURRENT_LIST_DIR}/MGSmartTesterMain.cpp"
        ${MG_ROOT_CPP}
        ${MG_DRIVER_CPP}
        ${MG_DRIVER_C}
    INCLUDE_DIRS
        "."
        "${MG_ROOT}"
        "${MG_ROOT}/src"
        "${MG_ROOT}/src/touch"
        "${MG_ROOT}/src/lcd"
        "${MG_ROOT}/src/fonts"
    REQUIRES
        arduino-esp32
        lvgl
        esp_lcd
        esp_driver_i2c
        esp_driver_gpio
        esp_timer
        esp_hw_support
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MG_MOBILE_PROFILE_IMPORT=1
    BOARD_HAS_PSRAM
    ESP_LCD_JD9165_VER_MAJOR=1
    ESP_LCD_JD9165_VER_MINOR=0
    ESP_LCD_JD9165_VER_PATCH=2
)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
