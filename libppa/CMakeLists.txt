cmake_minimum_required(VERSION 3.22)

add_library(libppa INTERFACE)
target_sources(libppa INTERFACE src/cond.c src/ppa.c)
target_include_directories(libppa INTERFACE include)
target_link_libraries(libppa INTERFACE
    pico_stdlib
    hardware_pwm
    hardware_dma
)
