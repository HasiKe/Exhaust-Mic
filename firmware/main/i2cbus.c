#include "i2cbus.h"
#include "pins.h"

static i2c_master_bus_handle_t BUS;

esp_err_t i2cbus_start(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = PIN_I2C_SCL,
        .sda_io_num = PIN_I2C_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        /* Auf der Platine sitzen Pullups (R40/R41), die internen bleiben
         * aus - sonst liegen sie parallel und der Bus wird zu schnell. */
        .flags.enable_internal_pullup = false,
    };
    return i2c_new_master_bus(&cfg, &BUS);
}

i2c_master_bus_handle_t i2cbus(void) { return BUS; }

esp_err_t i2cbus_geraet(uint8_t adresse, i2c_master_dev_handle_t *aus)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = adresse,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(BUS, &cfg, aus);
}
