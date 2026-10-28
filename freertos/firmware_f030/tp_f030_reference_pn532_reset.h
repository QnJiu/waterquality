#ifndef TP_F030_REFERENCE_PN532_RESET_H
#define TP_F030_REFERENCE_PN532_RESET_H
#include "tp_core.h"
/* PA6 drives PN532 RSTPD_N active-low, PB6/PB7 are the I2C lines.
 * Initialize before the scheduler; use step only in the door owner task after
 * the I2C transport has been aborted. Objects must persist for all retries. */
typedef struct { uint32_t since; uint8_t phase; bool initialized; } tp_f030_pn532_reset;
tp_result tp_f030_reference_pn532_reset_init(tp_f030_pn532_reset *);
/* Compatible with tp_f030_door_config.recover_hardware. Returns BUSY while
 * resetting. OK indicates reset timing and idle I2C lines, not card detection. */
tp_result tp_f030_reference_pn532_reset_step(void *);
#endif
