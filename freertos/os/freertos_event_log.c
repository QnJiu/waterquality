#include "tp_freertos_event_log.h"
#include <string.h>
static void run(void *ctx) {
    tp_freertos_event_log *log=ctx; tp_event e;
    for (;;) {
        if(xQueueReceive(log->queue,&e,portMAX_DELAY)!=pdPASS) continue;
        tp_result r=TP_OK;
        if(!log->journal.open) r=tp_event_log_open(&log->journal,&log->io);
        if(r==TP_OK) r=tp_event_log_append(&log->journal,&e);
        taskENTER_CRITICAL();
        if(r==TP_OK) log->stats.written++;
        else { log->stats.io_errors++; log->stats.dropped++; }
        taskEXIT_CRITICAL();
    }
}
tp_result tp_freertos_event_log_start(tp_freertos_event_log *log,const tp_event_log_io *io,
                                      uint16_t stack_words,UBaseType_t priority) {
    if(!log || !io || !io->read || !io->write || io->size<64 || io->size%32 || !stack_words ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    memset(log,0,sizeof(*log)); log->io=*io; log->queue=xQueueCreate(TP_EVENT_QUEUE_DEPTH,sizeof(tp_event));
    if(!log->queue) return TP_IO_ERROR;
    if(xTaskCreate(run,"tp-events",stack_words,log,priority,&log->task)!=pdPASS) {
        vQueueDelete(log->queue); log->queue=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}
tp_result tp_freertos_event_log_submit(tp_freertos_event_log *log,const tp_event *e) {
    if(!log || !log->queue || !e || !e->type) return TP_INVALID;
    BaseType_t ok=xQueueSend(log->queue,e,0);
    taskENTER_CRITICAL(); if(ok==pdPASS) log->stats.queued++; else log->stats.dropped++; taskEXIT_CRITICAL();
    return ok==pdPASS ? TP_OK : TP_BUSY;
}
void tp_freertos_event_log_stats(tp_freertos_event_log *log,tp_event_log_stats *out) {
    if(!log || !out) return;
    taskENTER_CRITICAL(); *out=log->stats; taskEXIT_CRITICAL();
}
