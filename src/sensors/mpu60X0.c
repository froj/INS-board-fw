#include <stdint.h>
#include <math.h>

#include "mpu60X0.h"
#include "mpu60X0_registers.h"


static uint8_t mpu60X0_reg_read(mpu60X0_t *dev, uint8_t reg)
{
    uint8_t ret = 0;
    if (dev->spi) {
        spiSelect(dev->spi);
        reg |= 0x80;
        spiSend(dev->spi, 1, &reg);
        spiReceive(dev->spi, 1, &ret);
        spiUnselect(dev->spi);
    }
    return ret;
}

static void mpu60X0_reg_write(mpu60X0_t *dev, uint8_t reg, uint8_t val)
{
    if (dev->spi) {
        spiSelect(dev->spi);
        uint8_t buf[] = {reg, val};
        spiSend(dev->spi, 2, buf);
        spiUnselect(dev->spi);
    }
}

static void mpu60X0_reg_read_multi(mpu60X0_t *dev, uint8_t reg, uint8_t *buf, int8_t len)
{
    if (dev->spi) {
        spiSelect(dev->spi);
        reg |= 0x80;
        spiSend(dev->spi, 1, &reg);
        spiReceive(dev->spi, len, buf);
        spiUnselect(dev->spi);
    }
}

#if 0
static void mpu60X0_reg_write_multi(mpu60X0_t *dev, uint8_t reg, const uint8_t *buf, int8_t len)
{
    if (dev->spi) {
        spiSelect(dev->spi);
        spiSend(dev->spi, 1, &reg);
        spiSend(dev->spi, len, buf);
        spiUnselect(dev->spi);
    }
}
#endif

void mpu60X0_init_using_spi(mpu60X0_t *dev, SPIDriver *spi_dev)
{
    dev->spi = spi_dev;
    dev->config = 0;
}

void mpu60X0_setup(mpu60X0_t *dev, int config)
{
    dev->config = config;
    // select gyro x as clock source and disable sleep
    mpu60X0_reg_write(dev, MPU60X0_RA_PWR_MGMT_1, MPU60X0_CLOCK_PLL_XGYRO);
    if (dev->spi) { // disable I2C interface
        mpu60X0_reg_write(dev, MPU60X0_RA_USER_CTRL, MPU60X0_USERCTRL_I2C_IF_DIS_BIT);
    }
    // gyro full scale
    mpu60X0_reg_write(dev, MPU60X0_RA_GYRO_CONFIG, (config<<1) & 0x18);
    // accelerometer full scale
    mpu60X0_reg_write(dev, MPU60X0_RA_ACCEL_CONFIG, (config<<3) & 0x18);
    // sample rate divisor
    mpu60X0_reg_write(dev, MPU60X0_RA_SMPLRT_DIV, (config >> 8) & 0xff);
    // enable interrupts: data ready
    mpu60X0_reg_write(dev, MPU60X0_RA_INT_ENABLE, MPU60X0_INTERRUPT_DATA_RDY);
    // low pass filter config, FSYNC disabled
    mpu60X0_reg_write(dev, MPU60X0_RA_CONFIG, (config>>16) & 0x07);
}

bool mpu60X0_ping(mpu60X0_t *dev)
{
    if (mpu60X0_reg_read(dev, MPU60X0_RA_WHO_AM_I) == 0x68) {
        return true;
    } else {
        return false;
    }
}

bool mpu60X0_self_test(mpu60X0_t *dev)
{
    (void)dev;
    return true; // TODO
}

static int32_t read_word(const uint8_t *buf) // signed int16
{
    return ((int16_t)((int8_t)buf[0]) << 8 | buf[1]);
}

void mpu60X0_read(mpu60X0_t *dev, float *gyro, float *acc, float *temp)
{
    static const float gyro_res[] = {(1/131.f)/180*M_PI, (1/65.5f)/180*M_PI, (1/32.8f)/180*M_PI, (1/16.4f)/180*M_PI}; // rad/s/LSB
    static const float acc_res[] = {1/16384.f, 1/8192.f, 1/4096.f, 1/2048.f}; // m/s^2 / LSB
    uint8_t buf[1 + 6 + 2 + 6]; // interrupt status, accel, temp, gyro
    mpu60X0_reg_read_multi(dev, MPU60X0_RA_INT_STATUS, buf, sizeof(buf));
    if (acc) {
        acc[0] = (float)read_word(&buf[1]) * gyro_res[dev->config & 0x3];
        acc[1] = (float)read_word(&buf[3]) * gyro_res[dev->config & 0x3];
        acc[2] = (float)read_word(&buf[5]) * gyro_res[dev->config & 0x3];
    }
    if (temp) {
        *temp = (float)read_word(&buf[7]) / 340 + 36.53;
    }
    if (gyro) {
        gyro[0] = (float)read_word(&buf[9]) * acc_res[(dev->config >> 2) & 0x3];
        gyro[1] = (float)read_word(&buf[11]) * acc_res[(dev->config >> 2) & 0x3];
        gyro[2] = (float)read_word(&buf[13]) * acc_res[(dev->config >> 2) & 0x3];
    }
}
