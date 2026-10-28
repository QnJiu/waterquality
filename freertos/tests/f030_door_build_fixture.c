/* Link fixture only. This is not a hardware or authentication implementation. */
#include "tp_f030_pn532_door.h"
#include "tp_f030_reference_pn532_reset.h"
static tp_f030_pn532_reset reset;
static tp_result lock(void *ctx,bool unlocked) { (void)ctx; (void)unlocked; return TP_IO_ERROR; }
static tp_result request(void *ctx,uint32_t id,const uint8_t *card,size_t n) {
    (void)ctx; (void)id; (void)card; (void)n; return TP_IO_ERROR;
}
tp_result tp_f030_door_board_prepare(tp_f030_door_config *c) {
    tp_result result=tp_f030_reference_pn532_reset_init(&reset);
    if(result!=TP_OK) return result;
    *c=(tp_f030_door_config){.lock={NULL,lock},.request=request,.command_timeout_ms=1000,
        .scan_interval_ms=100,.scan_retries=1,.unlock_ms=100,.freshness_ms=1000,
        .recovery_ctx=&reset,.recover_hardware=tp_f030_reference_pn532_reset_step};
    return TP_OK;
}
