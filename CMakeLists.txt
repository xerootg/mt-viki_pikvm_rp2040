cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
#include(/usr/share/pico-sdk/pico_sdk_init.cmake) # for *nix with a system-wide install
include(pico_sdk_init.cmake) # your responsible for setting PICO_SDK_PATH

project(mt-viki_pikvm_rp2040 C CXX ASM)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

set(CMAKE_CXX_STANDARD 11)

# include_directories(src/include)

file(GLOB SOURCES
    src/*.cpp
)
if (TARGET tinyusb_device)
    add_executable(mt-viki_pikvm_rp2040 ${SOURCES})

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

    # generate the header file into the source tree as it is included in the RP2040 datasheet
    pico_generate_pio_header(mt-viki_pikvm_rp2040 ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

    pico_set_program_name(mt-viki_pikvm_rp2040 "mt-viki_pikvm_rp2040")
    pico_set_program_version(mt-viki_pikvm_rp2040 "0.1")

    # Modify the below lines to enable/disable output over UART/USB
    pico_enable_stdio_uart(mt-viki_pikvm_rp2040 0)
    pico_enable_stdio_usb(mt-viki_pikvm_rp2040 1)

    # Add the standard library to the build
    target_link_libraries(mt-viki_pikvm_rp2040
            PRIVATE
            pico_stdlib
            hardware_pio)

    pico_add_extra_outputs(mt-viki_pikvm_rp2040)
elseif(PICO_ON_DEVICE)
    message("Skipping hello_usb because TinyUSB submodule is not initialized in the SDK")
endif()