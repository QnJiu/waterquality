#ifndef TP_F030_PN532_DOOR_H
#define TP_F030_PN532_DOOR_H

#include "tp_door_service.h"
#include "tp_pn532_i2c.h"

typedef struct {
    tp_door_io lock;
    void *request_ctx;
    tp_door_request request;
    uint32_t command_timeout_ms, scan_interval_ms;
    uint8_t scan_retries;
    uint32_t unlock_ms, freshness_ms, first_request_id;
} tp_f030_door_config;

typedef struct {
    tp_pn532_i2c transport;
    tp_door_service service;
} tp_f030_pn532_door;

/* bus and PN532 must already be recovered and synchronized. */
tp_result tp_f030_pn532_door_init(tp_f030_pn532_door *,const tp_pn532_i2c_bus *,
                                  const tp_f030_door_config *);
/* Owner task only, after bus recovery and physical PN532 reset/synchronization.
 * The subordinate's latched fault handler must be rearmed by its owner too. */
tp_result tp_f030_pn532_door_recover(tp_f030_pn532_door *);
/* Suitable for tp_f030_subordinate_on_door_fault. */
void tp_f030_pn532_door_abort(void *);

/* Supplied by the board. Configure the lock to its safe state and reset and
 * synchronize PN532 before returning TP_OK. Runs before the scheduler. */
tp_result tp_f030_door_board_prepare(tp_f030_door_config *);
/* Authentication task only; TP_OK means queued, not accepted. */
tp_result tp_f030_door_authorize(uint32_t request_id,bool approved);
#endif
