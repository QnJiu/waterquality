#include "tp_f030_subordinate.h"
#include "tp_f030_board_inputs.h"
#include "stm32f0xx.h"
#include "tp_f030_reference_lock.h"
#ifdef TP_F030_STOP_SLEEP
#include "tp_f030_stop.h"
#endif
#ifdef TP_F030_DOOR_CONFIG
#include "tp_f030_i2c.h"
#include "tp_f030_pn532_door.h"
static tp_f030_i2c i2c;
static tp_f030_pn532_door door;
static void *door_recovery_ctx;
static tp_result (*door_recover_hardware)(void *);
static tp_result recover_door(void *unused) {
    (void)unused;
    tp_result r=door_recover_hardware(door_recovery_ctx);
    if(r!=TP_OK) return r;
    r=tp_f030_i2c_init(&i2c);
    return r==TP_OK ? tp_f030_pn532_door_recover(&door) : r;
}
#endif

static tp_f030_subordinate subordinate;
volatile tp_result tp_f030_startup_result=TP_BUSY;
#ifdef TP_F030_DOOR_CONFIG
tp_result tp_f030_door_request_recovery(void) {
    return tp_f030_subordinate_recover_door(&subordinate);
}
tp_result tp_f030_door_authorize(uint32_t request_id,bool approved) {
    return tp_f030_subordinate_authorize(&subordinate,request_id,approved);
}
#endif

void tp_platform_assert(const char *file,int line) {
    (void)file; (void)line;
    __disable_irq();
    tp_f030_reference_lock_emergency_off();
    for(;;) { __NOP(); }
}
void vApplicationMallocFailedHook(void) { tp_platform_assert(__FILE__,__LINE__); }
void vApplicationStackOverflowHook(TaskHandle_t task,char *name) {
    (void)task; (void)name; tp_platform_assert(__FILE__,__LINE__);
}
int main(void) {
    SystemCoreClockUpdate();
    tp_door_service *service=NULL;
#ifdef TP_F030_DOOR_CONFIG
    tp_f030_door_config config={0};
    tp_f030_startup_result=tp_f030_door_board_prepare(&config);
    if(tp_f030_startup_result!=TP_OK) tp_platform_assert(__FILE__,__LINE__);
    tp_f030_startup_result=tp_f030_i2c_init(&i2c);
    if(tp_f030_startup_result!=TP_OK) tp_platform_assert(__FILE__,__LINE__);
    tp_pn532_i2c_bus bus=tp_f030_i2c_bus(&i2c);
    tp_f030_startup_result=tp_f030_pn532_door_init(&door,&bus,&config);
    if(tp_f030_startup_result!=TP_OK) tp_platform_assert(__FILE__,__LINE__);
    service=&door.service;
    door_recovery_ctx=config.recovery_ctx;
    door_recover_hardware=config.recover_hardware;
#endif
    tp_f030_startup_result=tp_f030_subordinate_init(&subordinate,NULL,tp_f030_board_inputs,
                                                      service,service!=NULL,384,tskIDLE_PRIORITY+2);
    if(tp_f030_startup_result!=TP_OK) tp_platform_assert(__FILE__,__LINE__);
#ifdef TP_F030_DOOR_CONFIG
    tp_f030_startup_result=tp_f030_subordinate_on_door_fault(&subordinate,tp_f030_pn532_door_abort,&door);
    if(tp_f030_startup_result!=TP_OK) tp_platform_assert(__FILE__,__LINE__);
    if(door_recover_hardware) {
        tp_f030_startup_result=tp_f030_subordinate_set_door_recovery(&subordinate,NULL,recover_door);
        if(tp_f030_startup_result!=TP_OK) tp_platform_assert(__FILE__,__LINE__);
    }
#endif
#ifdef TP_F030_STOP_SLEEP
    /* An unavailable LSE leaves the normal 1 ms tick running. */
    (void)tp_f030_stop_init();
#endif
    vTaskStartScheduler();
    tp_platform_assert(__FILE__,__LINE__);
}
