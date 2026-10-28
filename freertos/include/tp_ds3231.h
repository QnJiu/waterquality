#ifndef TP_DS3231_H
#define TP_DS3231_H
#include "tp_mb85rc16.h"
/* Reference DS3231 at fixed 7-bit I2C address 0x68, UTC years 2000..2099.
 * Use from a low-priority task: underlying I2C callbacks may block. Serialize
 * all access to the RTC, including time-setting, at the application level. */
typedef struct { tp_i2c_io io; } tp_ds3231;
tp_result tp_ds3231_init(tp_ds3231 *rtc, const tp_i2c_io *io);
/* OSF set, invalid BCD/date, unstable snapshot or bus errors return an error;
 * the output is changed only on success. */
tp_result tp_ds3231_read_utc(tp_ds3231 *rtc, int64_t *seconds);
/* Sets UTC and clears oscillator-stop flag only after writing the calendar.
 * Read back with tp_ds3231_read_utc to verify. */
tp_result tp_ds3231_set_utc(tp_ds3231 *rtc, int64_t seconds);
#endif
