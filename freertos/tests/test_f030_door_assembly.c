#include <assert.h>
#include <setjmp.h>
#define TP_F030_DOOR_CONFIG
#define __disable_irq() ((void)0)
#define __NOP() ((void)0)
#define SystemCoreClockUpdate() ((void)0)
#define tskIDLE_PRIORITY 0
#define vTaskStartScheduler() longjmp(started,1)
#define main firmware_main
static jmp_buf started;
#include "../firmware_f030/main_subordinate.c"
#undef main

void tp_f030_reference_lock_emergency_off(void) {}
static unsigned hardware_calls, i2c_calls, door_calls, registrations;
static tp_result hardware_result=TP_BUSY, i2c_result=TP_OK, door_result=TP_OK;
static tp_result (*registered_recovery)(void *);
static void *registered_ctx;
static tp_result hardware(void *ctx) { assert(ctx==&hardware_calls); hardware_calls++; return hardware_result; }
tp_result tp_f030_door_board_prepare(tp_f030_door_config *cfg) {
    *cfg=(tp_f030_door_config){.recovery_ctx=&hardware_calls,.recover_hardware=hardware}; return TP_OK;
}
tp_result tp_f030_i2c_init(tp_f030_i2c *i) { assert(i==&i2c); i2c_calls++; return i2c_result; }
tp_pn532_i2c_bus tp_f030_i2c_bus(tp_f030_i2c *i) { assert(i==&i2c); return (tp_pn532_i2c_bus){0}; }
tp_result tp_f030_pn532_door_init(tp_f030_pn532_door *d,const tp_pn532_i2c_bus *b,const tp_f030_door_config *c) {
    assert(d==&door && b && c); return TP_OK;
}
tp_result tp_f030_pn532_door_recover(tp_f030_pn532_door *d) { assert(d==&door); door_calls++; return door_result; }
void tp_f030_pn532_door_abort(void *ctx) { (void)ctx; }
tp_result tp_f030_board_inputs(void *ctx,tp_sub_inputs *out) { (void)ctx; (void)out; return TP_OK; }
tp_result tp_f030_subordinate_init(tp_f030_subordinate *s,void *ctx,tp_result (*read)(void *,tp_sub_inputs *),
    tp_door_service *d,bool fault,uint16_t stack,UBaseType_t priority) {
    assert(s==&subordinate && !ctx && read==tp_f030_board_inputs && d==&door.service && fault && stack && priority);
    return TP_OK;
}
tp_result tp_f030_subordinate_on_door_fault(tp_f030_subordinate *s,void (*fn)(void *),void *ctx) {
    assert(s==&subordinate && fn==tp_f030_pn532_door_abort && ctx==&door); return TP_OK;
}
tp_result tp_f030_subordinate_set_door_recovery(tp_f030_subordinate *s,void *ctx,tp_result (*fn)(void *)) {
    assert(s==&subordinate); registered_recovery=fn; registered_ctx=ctx; registrations++; return TP_OK;
}
tp_result tp_f030_subordinate_recover_door(tp_f030_subordinate *s) { assert(s==&subordinate); return TP_OK; }
tp_result tp_f030_subordinate_authorize(tp_f030_subordinate *s,uint32_t id,bool approved) {
    assert(s==&subordinate && id==42 && approved); return TP_OK;
}
int main(void) {
    if(!setjmp(started)) firmware_main();
    assert(registrations==1 && registered_recovery && i2c_calls==1);
    assert(tp_f030_door_request_recovery()==TP_OK && tp_f030_door_authorize(42,true)==TP_OK);
    assert(registered_recovery(registered_ctx)==TP_BUSY && i2c_calls==1 && door_calls==0);
    hardware_result=TP_IO_ERROR;
    assert(registered_recovery(registered_ctx)==TP_IO_ERROR && i2c_calls==1 && door_calls==0);
    hardware_result=TP_OK; i2c_result=TP_IO_ERROR;
    assert(registered_recovery(registered_ctx)==TP_IO_ERROR && i2c_calls==2 && door_calls==0);
    i2c_result=TP_OK; door_result=TP_IO_ERROR;
    assert(registered_recovery(registered_ctx)==TP_IO_ERROR && i2c_calls==3 && door_calls==1);
    door_result=TP_OK;
    assert(registered_recovery(registered_ctx)==TP_OK && i2c_calls==4 && door_calls==2);
    return 0;
}
