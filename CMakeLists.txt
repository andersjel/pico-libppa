cmake_minimum_required(VERSION 3.22)

# Configure paths.
include(vendor/pico-sdk/pico_sdk_init.cmake)
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include/boards)

# Setup project.
project(pico-audio)

# Configure compiler.
set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
add_compile_options(-Wall -Wno-int-conversion)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

pico_sdk_init()

add_executable(demo src/main.c src/ppa.c src/cond.c)
pico_add_extra_outputs(demo)

# Use USB for stdio.
pico_enable_stdio_uart(demo 0)
pico_enable_stdio_usb(demo 1)
target_compile_definitions(demo PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000
)

target_link_libraries(demo
    pico_stdlib
    hardware_pwm
    hardware_dma
)
