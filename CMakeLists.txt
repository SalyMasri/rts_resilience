cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rts_resilience C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(rts_resilience
    CyclicSched/main.c
    CyclicSched/schedule.c
    common/workload.c
)

target_include_directories(rts_resilience PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/CyclicSched
    ${CMAKE_CURRENT_LIST_DIR}/common
)

target_link_libraries(rts_resilience
    pico_stdlib
    hardware_timer
    hardware_gpio
    hardware_pwm
    hardware_spi
    hardware_i2c
    hardware_uart
    hardware_adc
    hardware_dma
)

pico_enable_stdio_usb(rts_resilience 1)
pico_enable_stdio_uart(rts_resilience 0)

pico_add_extra_outputs(rts_resilience)