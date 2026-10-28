#ifndef TP_F407_BOOT_BOARD_H
#define TP_F407_BOOT_BOARD_H
#include "tp_f407_boot.h"
/* Board code initializes safe outputs, supplies synchronous polling bus/image
 * access and immutable key; no scheduler. Returns failure if unconfigured. */
tp_result tp_f407_boot_board_options(tp_f407_boot_options *);
/* Stop peripheral interrupt sources and DMA, return clocks to application
 * startup expectations; failure prevents handoff. */
tp_result tp_f407_boot_board_quiesce(void);
#endif
