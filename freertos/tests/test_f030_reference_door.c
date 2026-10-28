#include "tp_f030_pn532_door.h"
#include "tp_f030_reference_cards.h"
#include "tp_f030_reference_lock.h"
#include "tp_f030_reference_pn532_reset.h"
#include <assert.h>

static const tp_door_card cards[] = {{{1,2,3,4},4}};
const tp_door_card *const tp_f030_cards=cards;
const size_t tp_f030_card_count=1;
static unsigned lock_init,reset_init,decisions;
static bool last_decision;
static tp_result submit_result=TP_OK;
static uint32_t last_id;
static tp_result lock(void *ctx,bool open) { (void)ctx; (void)open; return TP_OK; }
tp_result tp_f030_reference_lock_init(void) { lock_init++; return TP_OK; }
tp_door_io tp_f030_reference_lock_io(void) { return (tp_door_io){NULL,lock}; }
tp_result tp_f030_reference_pn532_reset_init(tp_f030_pn532_reset *r) {
    assert(r); reset_init++; return TP_OK;
}
tp_result tp_f030_reference_pn532_reset_step(void *ctx) { assert(ctx); return TP_BUSY; }
tp_result tp_f030_door_authorize(uint32_t id,bool approved) {
    decisions++; last_id=id; last_decision=approved; return submit_result;
}
int main(void) {
    tp_f030_door_config c={0};
    assert(tp_f030_door_board_prepare(NULL)==TP_INVALID && !lock_init && !reset_init);
    assert(tp_f030_door_board_prepare(&c)==TP_OK && lock_init==1 && reset_init==1);
    assert(c.lock.set_unlocked==lock && c.request && c.recover_hardware && c.recovery_ctx);
    const uint8_t known[]={1,2,3,4}, unknown[]={1,2,3,5};
    assert(c.request(c.request_ctx,42,known,sizeof(known))==TP_OK);
    assert(decisions==1 && last_id==42 && last_decision);
    assert(c.request(c.request_ctx,43,unknown,sizeof(unknown))==TP_OK);
    assert(decisions==2 && last_id==43 && !last_decision);
    submit_result=TP_BUSY;
    assert(c.request(c.request_ctx,44,known,sizeof(known))==TP_OK && decisions==3);
    submit_result=TP_IO_ERROR;
    assert(c.request(c.request_ctx,45,known,sizeof(known))==TP_IO_ERROR && decisions==4);
    assert(c.request(c.request_ctx,46,known,3)==TP_INVALID && decisions==4);
    return 0;
}
