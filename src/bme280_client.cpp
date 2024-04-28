#include "bme280_client.h"

// Need this define to support variable delay
#define __DELAY_BACKWARD_COMPATIBLE__

#include <TinyI2CMaster.h>
#include <assert.h>
#include <avr/io.h>
#include <util/delay.h>

#define F_SCL 100000 // 100khz

uint32_t bme_meas_delay_us_;
static uint8_t bme_dev_addr_ = BME280_I2C_ADDR_PRIM;
struct bme280_dev bme_dev_;

int8_t bme280_init() {
    int8_t rslt;

    bme_dev_.intf = BME280_I2C_INTF;
    bme_dev_.intf_ptr = &bme_dev_addr_;
    bme_dev_.read = bme280_i2c_read;
    bme_dev_.write = bme280_i2c_write;
    bme_dev_.delay_us = bme280_delay_us;

    TinyI2C.init();

    rslt = bme280_init(&bme_dev_);
    if (rslt != BME280_OK) {
        return rslt;
    }

    struct bme280_settings settings = {
        .osr_p = BME280_OVERSAMPLING_8X,
        .osr_t = BME280_OVERSAMPLING_1X,
        .osr_h = BME280_NO_OVERSAMPLING,
        .filter = BME280_FILTER_COEFF_2,
        .standby_time = 0, // Unused
    };
    rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &bme_dev_);
    if (rslt != BME280_OK) {
        return rslt;
    }

    rslt = bme280_cal_meas_delay(&bme_meas_delay_us_, &settings);
    return rslt;
};

int8_t bme280_measure(int32_t *pres) {
    // Start a measurement
    int8_t rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &bme_dev_);
    if (rslt != BME280_OK) {
        return rslt;
    }

    // Sleep until measurement is ready
    _delay_us(bme_meas_delay_us_);

    // Measure
    struct bme280_data data {};
    rslt = bme280_get_sensor_data(BME280_PRESS, &data, &bme_dev_);
    if (rslt != BME280_OK) {
        return rslt;
    }

    *pres = data.pressure;

    return 0;
}

BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length,
                                     void *intf_ptr) {

    if (!TinyI2C.start(*(uint8_t *)intf_ptr, 0)) {
        return BME280_E_COMM_FAIL;
    }

    if (!TinyI2C.write(reg_addr)) {
        return BME280_E_COMM_FAIL;
    }

    if (!TinyI2C.restart(*(uint8_t *)intf_ptr, length)) {
        return BME280_E_COMM_FAIL;
    }

    for (size_t i = 0; i < length; i++) {
        reg_data[i] = TinyI2C.read();
    }

    TinyI2C.stop();

    return BME280_OK;
}

BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length,
                                      void *intf_ptr) {
    if (!TinyI2C.start(*(uint8_t *)intf_ptr, 0)) {
        return BME280_E_COMM_FAIL;
    }

    for (size_t i = 0; i < length; i++) {
        if (!TinyI2C.write(reg_addr + i)) {
            return BME280_E_COMM_FAIL;
        }

        if (!TinyI2C.write(reg_data[i])) {
            return BME280_E_COMM_FAIL;
        }
    }

    TinyI2C.stop();

    return BME280_OK;
}

void bme280_delay_us(uint32_t period_us, void *intf_ptr) { _delay_us(period_us); }
