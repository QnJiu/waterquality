#include "tp_f407_upgrade_service.h"
#include "tp_event_transition.h"
#include <assert.h>
#include <string.h>
static tp_f407_upgrade_service *expected;
static int stage,fail_at,scheduler,context;
static tp_f407_app app;
static bool app_ok,board_ok;
static int app_calls,board_calls,logged;
static tp_event last_event;
tp_result tp_freertos_event_log_submit(tp_freertos_event_log *log,const tp_event *event) {
    assert(log==&app.events); logged++; last_event=*event; return TP_OK;
}
bool tp_f407_app_trial_healthy(tp_f407_app *a) {
    assert(a==&app); app_calls++; return app_ok;
}
tp_result tp_runtime_request_maintenance(tp_freertos_runtime *rt) { assert(rt==&app.runtime); return TP_LOCK_BUSY; }
static bool lock(void *p) { return p==&context; }
static void unlock(void *p) { assert(p==&context); }
static bool health(void *p) { assert(p==&context); board_calls++; return board_ok; }
static void done(void *p,tp_result r) { assert(p==&context); (void)r; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
tp_result tp_f407_upgrade_init(tp_f407_upgrade *u,const tp_f407_upgrade_options *o) {
    assert(u==&expected->upgrade && o->product_id==42 && stage==0); stage=1;
    if(fail_at==1) return TP_IO_ERROR;
    u->running_bank=1; u->boot_at_start.phase=TP_BOOT_TRIAL;
    u->boot_at_start.candidate_version=0x12345678; return TP_OK;
}
tp_result tp_freertos_trial_start(tp_freertos_trial *t,const tp_boot_io *boot,uint8_t bank,
    uint32_t stable,uint32_t gap,uint32_t period,uint32_t timeout,uint16_t stack,
    UBaseType_t priority,void *ctx,bool (*l)(void *),void (*ul)(void *),
    bool (*h)(void *),void (*f)(void *,tp_result)) {
    assert(stage==1 && t==&expected->trial && boot==&expected->upgrade.secure.boot && bank==1);
    assert(stable==100 && gap==40 && period==20 && timeout==300 && stack==512 && priority==1);
    assert(ctx==expected && l(ctx)); ul(ctx);
    app_ok=false; board_ok=true; assert(!h(ctx));
    app_ok=true; board_ok=false; assert(!h(ctx));
    board_ok=true; assert(h(ctx));
    assert(app_calls==3 && board_calls==3); app_calls=board_calls=0;
    uint16_t version[3];
    assert(tp_service_read_holding(&app.service,119,3,version)==TP_OK);
    assert(version[0]==0 && version[1]==0 && version[2]==0);
    f(ctx,TP_IO_ERROR);
    assert(last_event.type==TP_EVENT_UPGRADE_TRIAL && last_event.detail==TP_IO_ERROR &&
           last_event.value==0x12345678 && !last_event.utc_valid);
    assert(tp_service_read_holding(&app.service,119,3,version)==TP_OK && version[0]==0);
    int before=logged; f(ctx,TP_OK);
    assert(logged==before+1 && last_event.type==TP_EVENT_UPGRADE_TRIAL &&
           last_event.detail==TP_OK && last_event.value==0x12345678);
    assert(tp_service_read_holding(&app.service,119,3,version)==TP_OK);
    assert(version[0]==1 && version[1]==0x1234 && version[2]==0x5678);
    stage=2;
    return fail_at==2 ? TP_IO_ERROR : TP_OK;
}
tp_result tp_f407_upgrade_uart_init_guarded(tp_f407_upgrade_uart *u,const tp_upgrade_commands *commands,
    uint32_t timeout,uint16_t stack,UBaseType_t priority,void *ctx,
    bool (*l)(void *),void (*ul)(void *),void *maintenance_ctx,tp_result (*gate)(void *)) {
    assert(maintenance_ctx==&app.runtime && gate(maintenance_ctx)==TP_LOCK_BUSY);
    assert(stage==2 && u==&expected->uart && commands==&expected->upgrade.secure.commands);
    assert(timeout==50 && stack==1024 && priority==2 && ctx==&context && l==lock && ul==unlock); stage=3;
    return fail_at==3 ? TP_IO_ERROR : TP_OK;
}
int main(void) {
    tp_f407_upgrade_service s; expected=&s;
    app.runtime.owner=&app.runtime;
    tp_f407_upgrade_service_options o={.storage={.product_id=42},.runtime=&app.runtime,.app=&app,.ctx=&context,
        .try_lock=lock,.unlock=unlock,.healthy=health,.finished=done,.frame_timeout_ms=50,
        .stable_ms=100,.max_gap_ms=40,.period_ms=20,.trial_timeout_ms=300,
        .uart_stack_words=1024,.trial_stack_words=512,.uart_priority=2,.trial_priority=1};
    assert(tp_f407_upgrade_service_start(NULL,&o)==TP_INVALID);
    o.runtime=NULL; assert(tp_f407_upgrade_service_start(&s,&o)==TP_INVALID); o.runtime=&app.runtime;
    o.app=NULL; assert(tp_f407_upgrade_service_start(&s,&o)==TP_INVALID); o.app=&app;
    scheduler=1; assert(tp_f407_upgrade_service_start(&s,&o)==TP_INVALID && stage==0); scheduler=0;
    o.healthy=NULL; assert(tp_f407_upgrade_service_start(&s,&o)==TP_INVALID && stage==0); o.healthy=health;
    for(int i=1;i<=3;i++) {
        memset(&app.service,0,sizeof(app.service)); app.service.controller=&app.controller;
        stage=0; fail_at=i; assert(tp_f407_upgrade_service_start(&s,&o)==TP_IO_ERROR && stage==i);
    }
    memset(&app.service,0,sizeof(app.service)); app.service.controller=&app.controller;
    stage=0; fail_at=0; assert(tp_f407_upgrade_service_start(&s,&o)==TP_OK && stage==3);
    tp_staged_guard guard;
    assert(tp_f407_upgrade_staged_init(NULL,&guard)==TP_INVALID);
    assert(tp_f407_upgrade_staged_init(&s,&guard)==TP_OK);
    assert(tp_staged_guard_begin(&guard,NULL,NULL,0)==TP_LOCK_BUSY);
    assert(!guard.active);
    return 0;
}
