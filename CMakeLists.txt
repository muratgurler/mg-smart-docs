cmake_minimum_required(VERSION 3.16)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")

get_filename_component(MG_P4_PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
add_compile_definitions(
    LV_CONF_PATH=${MG_P4_PROJECT_DIR}/lv_conf_p4.h
    LV_LVGL_H_INCLUDE_SIMPLE
)
list(APPEND compile_definitions "ARDUINO_USB_MODE=1")
list(APPEND compile_definitions "ARDUINO_USB_CDC_ON_BOOT=1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mg_smart_tester_p4_core)
message(STATUS "MG Smart Tester Beta 1.0 Fix1: MCP23S17 HAL + safe routed scan backend")
