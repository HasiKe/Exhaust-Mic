/* Der gemeinsame I2C-Strang: PCM1863 (0x4A) und DS3231 (0x68). */
#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t i2cbus_start(void);
i2c_master_bus_handle_t i2cbus(void);
esp_err_t i2cbus_geraet(uint8_t adresse, i2c_master_dev_handle_t *aus);
