cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must come before project())
include(pico_sdk_import.cmake)

project(pico2_ili9341 C CXX ASM)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

# Initialise the Pico SDK
pico_sdk_init()

add_subdirectory(src)
