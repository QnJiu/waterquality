/* Link fixture only. This is not a hardware or authentication implementation. */
#include "tp_f030_pn532_door.h"
static tp_result lock(void *ctx,bool unlocked) { (void)ctx; (void)unlocked; return TP_IO_ERROR; }
static tp_result request(void *ctx,uint32_t id,const uint8_t *card,size_t n) {
    (void)ctx; (void)id; (void)card; (void)n; return TP_IO_ERROR;
}
static tp_result recover_hardware(void *ctx) { (void)ctx; return TP_IO_ERROR; }
tp_result tp_f030_door_board_prepare(tp_f030_door_config *c) {
    *c=(tp_f030_door_config){.lock={NULL,lock},.request=request,.command_timeout_ms=1000,
        .scan_interval_ms=100,.scan_retries=1,.unlock_ms=100,.freshness_ms=1000,
        .recover_hardware=recover_hardware};
    return TP_OK;
}
