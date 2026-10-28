#include "tp_f030_pn532_door.h"
#include "tp_f030_reference_cards.h"
#include "tp_f030_reference_lock.h"
#include "tp_f030_reference_pn532_reset.h"

static tp_f030_pn532_reset reset;
static tp_door_allowlist allowlist;
static tp_result submit(void *ctx,uint32_t id,bool approved) {
    (void)ctx;
    tp_result r=tp_f030_door_authorize(id,approved);
    /* During reader verification the owner rejects decisions. Ignore that
     * card until it is removed and presented again; never open on BUSY. */
    return r==TP_BUSY ? TP_OK : r;
}
tp_result tp_f030_door_board_prepare(tp_f030_door_config *config) {
    if (!config) return TP_INVALID;
    tp_result r=tp_door_allowlist_init(&allowlist,tp_f030_cards,tp_f030_card_count,NULL,submit);
    if (r!=TP_OK) return r;
    r=tp_f030_reference_lock_init();
    if (r!=TP_OK) return r;
    r=tp_f030_reference_pn532_reset_init(&reset);
    if (r!=TP_OK) return r;
    *config=(tp_f030_door_config){
        .lock=tp_f030_reference_lock_io(), .request_ctx=&allowlist,
        .request=tp_door_allowlist_request, .command_timeout_ms=1000,
        .scan_interval_ms=100, .scan_retries=1, .unlock_ms=500,
        .freshness_ms=1000, .first_request_id=1,
        .recovery_ctx=&reset, .recover_hardware=tp_f030_reference_pn532_reset_step
    };
    return TP_OK;
}
