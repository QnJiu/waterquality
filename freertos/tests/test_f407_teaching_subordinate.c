#include "tp_f407_board_config.h"
#include "tp_f407_uart4_subordinate.h"
#include <assert.h>

static tp_result next_result=TP_OK;
static unsigned calls;
tp_result tp_f407_uart4_subordinate_init(tp_f407_uart4_subordinate *uart,
    uint32_t period,uint32_t timeout,uint32_t freshness,uint32_t token,
    uint16_t stack,UBaseType_t priority) {
    calls++;
    assert(period==500 && timeout==200 && freshness==1500 && token==1);
    assert(stack==512 && priority==2);
    if(next_result==TP_OK) uart->subordinate.snapshot=(QueueHandle_t)uart;
    return next_result;
}
int main(void) {
    tp_f407_app_options options;
    assert(tp_f407_board_options(NULL)==TP_INVALID && calls==0);
    next_result=TP_IO_ERROR;
    assert(tp_f407_board_options(&options)==TP_IO_ERROR && calls==1);
    next_result=TP_OK;
    assert(tp_f407_board_options(&options)==TP_OK && calls==2);
    assert(options.subordinate && options.subordinate->snapshot);
    assert(options.interlocks_ok(options.interlocks_ctx));
    assert(options.resource_faults(options.resource_ctx)==0);
    assert(options.recipe && options.recipe_len && options.auto_calibration && options.auto_standard);
    return 0;
}
