set(TARGET pico2_ili9341)

add_executable(${TARGET}
    main.c
    ili9488.c
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET}
    pico_stdlib
    hardware_spi
    hardware_gpio
)

# Produce .uf2 for drag-and-drop flashing
pico_add_extra_outputs(${TARGET})

# Route stdio to USB-CDC so printf works over USB (optional debug)
pico_enable_stdio_usb(${TARGET} 1)
pico_enable_stdio_uart(${TARGET} 0)
