cmake_minimum_required(VERSION 3.25)
include(vendor/pico-sdk/pico_sdk_init.cmake)

project(pico-audio)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

add_compile_options(-Wall)

pico_sdk_init()

add_executable(demo src/main.cpp)
pico_add_extra_outputs(demo)

# Use USB for stdio.
pico_enable_stdio_uart(demo 0)
pico_enable_stdio_usb(demo 1)
target_compile_definitions(demo PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000)

target_link_libraries(demo pico_stdlib)
