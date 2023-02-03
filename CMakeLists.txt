cmake_minimum_required(VERSION 3.22)

# Configure paths.
include(vendor/pico-sdk/pico_sdk_init.cmake)
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include/boards)

# Setup project.
project(libppa)

# Configure compiler.
set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
add_compile_options(-Wall -Wno-int-conversion)
include_directories(include)

pico_sdk_init()

add_subdirectory(vendor/picomp3lib/src)
add_subdirectory(libppa)

add_custom_command(
    OUTPUT strauss.mp3
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/strauss.ogg
    COMMAND
        ffmpeg
            -i ${CMAKE_CURRENT_SOURCE_DIR}/resources/strauss.ogg
            -write_xing 0
            -id3v2_version 0
            -q 5
            -ar 22050
            -ac 1
            -y
            strauss.mp3
    WORKING_DIRECTORY .
)

add_custom_command(
    OUTPUT strauss.o
    MAIN_DEPENDENCY strauss.mp3
    COMMAND
       arm-none-eabi-objcopy
        -I binary
        -O elf32-littlearm
        --rename-section .data=.flashdata,readonly,contents,alloc
        strauss.mp3
        strauss.o
)

add_executable(libppa-demo src/main.c strauss.o)
pico_add_extra_outputs(libppa-demo)

# Use USB for stdio.
pico_enable_stdio_uart(libppa-demo 0)
pico_enable_stdio_usb(libppa-demo 1)
target_compile_definitions(libppa-demo PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000
)

target_link_libraries(libppa-demo
    pico_stdlib
    hardware_pwm
    hardware_dma
    libppa
    picomp3lib
)
