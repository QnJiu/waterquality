#include "tp_freertos_stage_transport.h"
#include <string.h>
#if configTICK_RATE_HZ != 1000
#error "stage transport requires a 1 kHz tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit ticks required");
#define BYTE_BUDGET 64u
static tp_result send_frame(void *ctx,const uint8_t *data,size_t n) {
    tp_freertos_stage_transport *t=ctx;
    return t->io.send(t->io.ctx,data,n)==TP_OK ? TP_OK : TP_IO_ERROR;
}
static tp_result execute(void *ctx,const tp_upgrade_commands *commands,tp_upgrade_command cmd,
                         const uint8_t *data,size_t n,tp_upgrade_reply *reply) {
    tp_freertos_stage_transport *t=ctx;
    tp_result r=tp_stage_session_execute(t->session,commands,cmd,data,n,reply);
    if(cmd==TP_UP_CMD_FINISH && r==TP_OK) t->pending=true;
    return r;
}
static void reset(tp_freertos_stage_transport *t) {
    tp_stage_session_abort(t->session); tp_upgrade_stream_reset(&t->stream);
    t->pending=false; t->installing=false;
}
void tp_freertos_stage_transport_step(tp_freertos_stage_transport *t,uint32_t now) {
    if(!t || !t->session || !t->io.receive) return;
    if(t->pending) {
        if(!t->installing && (uint32_t)(now-t->last_byte_ms)>=t->idle_ms) {
            reset(t); t->io.finished(t->io.ctx,TP_TIMEOUT); return;
        }
        tp_result r=t->installing ? tp_stage_session_install_step(t->session) :
                                   tp_stage_session_install_begin(t->session);
        if(!t->installing && r==TP_OK) { t->installing=true; return; }
        if(r!=TP_BUSY) { reset(t); t->io.finished(t->io.ctx,r); }
        return;
    }
    for(unsigned i=0;i<BYTE_BUDGET;i++) {
        uint8_t b; tp_result r=t->io.receive(t->io.ctx,&b);
        if(r==TP_BUSY) break;
        if(r!=TP_OK) { reset(t); return; }
        t->last_byte_ms=now;
        r=tp_upgrade_stream_byte(&t->stream,b,now);
        /* Invalid input is ignored; any send failure closes the session. */
        if(r!=TP_OK && r!=TP_INVALID) { reset(t); return; }
        if(t->pending) return; /* response has been sent before installation */
    }
    tp_upgrade_stream_poll(&t->stream,now);
    if(t->session->locked && (uint32_t)(now-t->last_byte_ms)>=t->idle_ms) reset(t);
}
static void task(void *arg) {
    tp_freertos_stage_transport *t=arg;
    for(;;) { tp_freertos_stage_transport_step(t,(uint32_t)xTaskGetTickCount()); vTaskDelay(pdMS_TO_TICKS(t->period_ms)); }
}
tp_result tp_freertos_stage_transport_start(tp_freertos_stage_transport *t,tp_stage_session *s,
    const tp_stage_transport_io *io,uint32_t frame,uint32_t idle,uint32_t period,
    uint16_t stack,UBaseType_t priority) {
    if(!t || !s || !s->upgrade || s->locked || !io || !io->receive || !io->send || !io->finished ||
       !frame || !idle || idle>=0x80000000u || !period || period>=idle || !stack ||
       priority>=configMAX_PRIORITIES || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    memset(t,0,sizeof(*t)); t->session=s; t->io=*io; t->idle_ms=idle; t->period_ms=period;
    tp_result r=tp_upgrade_stream_init(&t->stream,&t->placeholder,t,send_frame,frame);
    if(r!=TP_OK) return r;
    tp_upgrade_wire_set_executor(&t->stream.wire,t,execute);
    if(xTaskCreate(task,"tp-stage",stack,t,priority,&t->task)!=pdPASS) { memset(t,0,sizeof(*t)); return TP_IO_ERROR; }
    return TP_OK;
}
