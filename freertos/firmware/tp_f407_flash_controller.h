#ifndef TP_F407_FLASH_CONTROLLER_H
#define TP_F407_FLASH_CONTROLLER_H
#include "tp_f407_image_flash.h"
/* Nonblocking scheduling is not provided: internal Flash operations can stall
 * code fetch on the single-bank F407. Run with outputs made safe and a watchdog
 * strategy compatible with the worst-case data-sheet erase time. */
typedef struct { uint32_t poll_limit; bool poisoned; } tp_f407_flash_controller;
/* Initialize once per system boot. After TP_TIMEOUT reset the device; do not
 * reinitialize while the previous flash operation may still be running. */
tp_result tp_f407_flash_controller_init(tp_f407_flash_controller *,uint32_t poll_limit,tp_f407_flash_ops *);
#endif
