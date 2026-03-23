/*
 * touch.c — Capacitive touch (FT6236 / GT911 auto-detect)
 */
#include "touch.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define FT6236_ADDR   0x38
#define GT911_ADDR_A  0x5D
#define GT911_ADDR_B  0x14

#define TOUCH_I2C_INST i2c0
#define TOUCH_I2C_BAUD 400000

static uint8_t _addr = 0;
static bool _was_pressed = false;
static bool _tap_evt = false;

static bool i2c_probe(uint8_t addr)
{
    uint8_t dummy;
    return i2c_read_blocking(TOUCH_I2C_INST, addr, &dummy, 1, false) >= 0;
}

static int i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    int ret = i2c_write_blocking(TOUCH_I2C_INST, addr, &reg, 1, true);
    if (ret < 0) return ret;
    return i2c_read_blocking(TOUCH_I2C_INST, addr, buf, len, false);
}

bool touch_init(void)
{
    i2c_init(TOUCH_I2C_INST, TOUCH_I2C_BAUD);
    gpio_set_function(PIN_TP_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_TP_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_TP_SDA);
    gpio_pull_up(PIN_TP_SCL);

    gpio_init(PIN_TP_RST);
    gpio_set_dir(PIN_TP_RST, GPIO_OUT);
    gpio_put(PIN_TP_RST, 0); sleep_ms(20);
    gpio_put(PIN_TP_RST, 1); sleep_ms(100);

    gpio_init(PIN_TP_INT);
    gpio_set_dir(PIN_TP_INT, GPIO_IN);
    gpio_pull_up(PIN_TP_INT);

    sleep_ms(50);

    if (i2c_probe(FT6236_ADDR))  { _addr = FT6236_ADDR;  return true; }
    if (i2c_probe(GT911_ADDR_A)) { _addr = GT911_ADDR_A;  return true; }
    if (i2c_probe(GT911_ADDR_B)) { _addr = GT911_ADDR_B;  return true; }

    return false;
}

static touch_point_t read_ft6236(void)
{
    touch_point_t pt = {0, 0, false};
    uint8_t buf[5];

    if (i2c_read_reg(_addr, 0x02, buf, 1) < 0) return pt;
    uint8_t n = buf[0] & 0x0F;
    if (n == 0 || n > 2) return pt;

    if (i2c_read_reg(_addr, 0x03, buf, 4) < 0) return pt;
    pt.x = ((buf[0] & 0x0F) << 8) | buf[1];
    pt.y = ((buf[2] & 0x0F) << 8) | buf[3];
    pt.pressed = true;
    return pt;
}

static touch_point_t read_gt911(void)
{
    touch_point_t pt = {0, 0, false};

    uint8_t reg[2] = {0x81, 0x4E};
    uint8_t status;
    i2c_write_blocking(TOUCH_I2C_INST, _addr, reg, 2, true);
    if (i2c_read_blocking(TOUCH_I2C_INST, _addr, &status, 1, false) < 0) return pt;

    if (!(status & 0x80) || (status & 0x0F) == 0) {
        uint8_t clr[3] = {0x81, 0x4E, 0x00};
        i2c_write_blocking(TOUCH_I2C_INST, _addr, clr, 3, false);
        return pt;
    }

    uint8_t cr[2] = {0x81, 0x50};
    uint8_t buf[4];
    i2c_write_blocking(TOUCH_I2C_INST, _addr, cr, 2, true);
    if (i2c_read_blocking(TOUCH_I2C_INST, _addr, buf, 4, false) < 0) return pt;

    pt.x = buf[0] | (buf[1] << 8);
    pt.y = buf[2] | (buf[3] << 8);
    pt.pressed = true;

    uint8_t clr[3] = {0x81, 0x4E, 0x00};
    i2c_write_blocking(TOUCH_I2C_INST, _addr, clr, 3, false);
    return pt;
}

touch_point_t touch_read(void)
{
    touch_point_t pt = {0, 0, false};
    if (_addr == FT6236_ADDR) pt = read_ft6236();
    else if (_addr == GT911_ADDR_A || _addr == GT911_ADDR_B) pt = read_gt911();

    /* Tap detection: fire on release after a press */
    if (pt.pressed) {
        _was_pressed = true;
    } else if (_was_pressed) {
        _was_pressed = false;
        _tap_evt = true;
    }

    return pt;
}

bool touch_tapped(void)
{
    bool t = _tap_evt;
    _tap_evt = false;
    return t;
}
