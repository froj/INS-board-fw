#include <ch.h>
#include <string.h>
#include <memstreams.h>
#include <chprintf.h>
#include <ff.h>
#include "main.h"
#include "sensors/onboardsensors.h"

#include "sdlog.h"

#define EVENT_MASK_MPU6000 1

static THD_WORKING_AREA(sdlog_wa, 512);
static THD_FUNCTION(sdlog, arg)
{
    (void)arg;
    chRegSetThreadName("sdlog");
    static event_listener_t sensor_listener;
    chEvtRegisterMaskWithFlags(&sensor_events, &sensor_listener,
                               (eventmask_t)EVENT_MASK_MPU6000,
                               (eventflags_t)SENSOR_EVENT_MPU6000);
    bool error = false;
    UINT _bytes_written;

    static FIL mpu6000_fd;
    FRESULT res = f_open(&mpu6000_fd, "/log/mpu6000.csv", FA_WRITE | FA_CREATE_ALWAYS);
    if (res) {
        chprintf(stdout, "error %d opening %s\n", res, "/log/mpu6000.csv");
        return -1;
    }
    const char *mpu_descr = "time,gyro_x,gyro_y,gyro_z,acc_x,acc_y,acc_z\n";
    error = error || f_write(&mpu6000_fd, mpu_descr, strlen(mpu_descr), &_bytes_written);

    while (!error) {
        static uint8_t writebuf[200];
        static MemoryStream writebuf_stream;
        eventmask_t events = chEvtWaitAny(EVENT_MASK_MPU6000);
        float t = (float)chVTGetSystemTimeX() / CH_CFG_ST_FREQUENCY;

        if (events & EVENT_MASK_MPU6000) {
            chSysLock();
            float gx = mpu_gyro_sample.rate[0];
            float gy = mpu_gyro_sample.rate[1];
            float gz = mpu_gyro_sample.rate[2];
            float ax = mpu_acc_sample.acceleration[0];
            float ay = mpu_acc_sample.acceleration[1];
            float az = mpu_acc_sample.acceleration[2];
            chSysUnlock();
            msObjectInit(&writebuf_stream, writebuf, sizeof(writebuf), 0);
            chprintf((BaseSequentialStream*)&writebuf_stream,
                      "%f,%f,%f,%f,%f,%f,%f\n", t, gx, gy, gz, ax, ay, az);
            UINT _bytes_written;
            int ret = f_write(&mpu6000_fd, writebuf, writebuf_stream.eos, &_bytes_written);
            if (ret != 0) {
                chprintf(stdout, "write failed %d\n", ret);
            }
            if (ret == 9) {
                f_open(&mpu6000_fd, "/log/mpu6000.csv", FA_WRITE);
                f_lseek(&mpu6000_fd, f_size(&mpu6000_fd));
            }
            static int sync_needed = 0;
            sync_needed++;
            if (sync_needed == 100) {
                f_sync(&mpu6000_fd);
                sync_needed = 0;
            }
        }
    }
    return -1;
}

void sdlog_start(void)
{
    chThdCreateStatic(sdlog_wa, sizeof(sdlog_wa), LOWPRIO, sdlog, NULL);
}