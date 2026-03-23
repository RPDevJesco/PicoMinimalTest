set(TARGET pico2_ili9341)

add_executable(${TARGET}
    main.c
    framebuffer.c
    display_spi.c
    input.c
    buzzer.c
    joystick.c
    touch.c
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${TARGET} PRIVATE
    SOC_RP2350=1
)

target_link_libraries(${TARGET}
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_i2c
    hardware_pwm
    hardware_adc
)

pico_add_extra_outputs(${TARGET})

pico_enable_stdio_usb(${TARGET} 0)
pico_enable_stdio_uart(${TARGET} 0)
