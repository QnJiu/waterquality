#include "tp_f407_upgrade_service.h"
#include <assert.h>
#include <string.h>
static tp_f407_upgrade_service *expected;
static int stage,fail_at,scheduler,context;
static tp_freertos_runtime runtime;
tp_result tp_runtime_request_maintenance(tp_freertos_runtime *rt) { assert(rt==&runtime); return TP_LOCK_BUSY; }
static bool lock(void *p) { return p==&context; }
static void unlock(void *p) { assert(p==&context); }
static bool health(void *p) { return p==&context; }
static void done(void *p,tp_result r) { assert(p==&context); (void)r; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
tp_result tp_f407_upgrade_init(tp_f407_upgrade *u,const tp_f407_upgrade_options *o) {
    assert(u==&expected->upgrade && o->product_id==42 && stage==0); stage=1;
    if(fail_at==1) return TP_IO_ERROR;
    u->running_bank=1; return TP_OK;
}
tp_result tp_freertos_trial_start(tp_freertos_trial *t,const tp_boot_io *boot,uint8_t bank,
    uint32_t stable,uint32_t gap,uint32_t period,uint32_t timeout,uint16_t stack,
    UBaseType_t priority,void *ctx,bool (*l)(void *),void (*ul)(void *),
    bool (*h)(void *),void (*f)(void *,tp_result)) {
    assert(stage==1 && t==&expected->trial && boot==&expected->upgrade.secure.boot && bank==1);
    assert(stable==100 && gap==40 && period==20 && timeout==300 && stack==512 && priority==1);
    assert(ctx==&context && l==lock && ul==unlock && h==health && f==done); stage=2;
    return fail_at==2 ? TP_IO_ERROR : TP_OK;
}
tp_result tp_f407_upgrade_uart_init_guarded(tp_f407_upgrade_uart *u,const tp_upgrade_commands *commands,
    uint32_t timeout,uint16_t stack,UBaseType_t priority,void *ctx,
    bool (*l)(void *),void (*ul)(void *),void *maintenance_ctx,tp_result (*gate)(void *)) {
    assert(maintenance_ctx==&runtime && gate(maintenance_ctx)==TP_LOCK_BUSY);
    assert(stage==2 && u==&expected->uart && commands==&expected->upgrade.secure.commands);
    assert(timeout==50 && stack==1024 && priority==2 && ctx==&context && l==lock && ul==unlock); stage=3;
    return fail_at==3 ? TP_IO_ERROR : TP_OK;
}
int main(void) {
    tp_f407_upgrade_service s; expected=&s;
    runtime.owner=&runtime;
    tp_f407_upgrade_service_options o={.storage={.product_id=42},.runtime=&runtime,.ctx=&context,
        .try_lock=lock,.unlock=unlock,.healthy=health,.finished=done,.frame_timeout_ms=50,
        .stable_ms=100,.max_gap_ms=40,.period_ms=20,.trial_timeout_ms=300,
        .uart_stack_words=1024,.trial_stack_words=512,.uart_priority=2,.trial_priority=1};
    assert(tp_f407_upgrade_service_start(NULL,&o)==TP_INVALID);
    o.runtime=NULL; assert(tp_f407_upgrade_service_start(&s,&o)==TP_INVALID); o.runtime=&runtime;
    scheduler=1; assert(tp_f407_upgrade_service_start(&s,&o)==TP_INVALID && stage==0); scheduler=0;
    o.healthy=NULL; assert(tp_f407_upgrade_service_start(&s,&o)==TP_INVALID && stage==0); o.healthy=health;
    for(int i=1;i<=3;i++) { stage=0; fail_at=i; assert(tp_f407_upgrade_service_start(&s,&o)==TP_IO_ERROR && stage==i); }
    stage=0; fail_at=0; assert(tp_f407_upgrade_service_start(&s,&o)==TP_OK && stage==3);
    return 0;
}
