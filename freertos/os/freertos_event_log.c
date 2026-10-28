#include "tp_freertos_event_log.h"
#include <string.h>
static void run(void *ctx) {
    tp_freertos_event_log *log=ctx; tp_event_log_job job;
    for (;;) {
        if(xQueueReceive(log->queue,&job,portMAX_DELAY)!=pdPASS) continue;
        tp_result r=TP_OK;
        if(!log->journal.open) r=tp_event_log_open(&log->journal,&log->io);
        if(job.query) {
            tp_event_log_reply reply={0};
            if(r==TP_OK && job.latest && !log->journal.has_records) r=TP_INVALID;
            if(r==TP_OK) r=tp_event_log_read(&log->journal,
                job.latest ? log->journal.latest_sequence : job.sequence,&reply.event);
            reply.result=r;
            if(r!=TP_OK && r!=TP_INVALID) { taskENTER_CRITICAL(); log->stats.io_errors++; taskEXIT_CRITICAL(); }
            /* Exactly one query may be outstanding; a full reply queue means
             * the client violated that contract, so wait instead of losing it. */
            (void)xQueueSend(log->replies,&reply,portMAX_DELAY);
        } else {
            if(r==TP_OK) r=tp_event_log_append(&log->journal,&job.event);
            taskENTER_CRITICAL();
            if(r==TP_OK) log->stats.written++;
            else { log->stats.io_errors++; log->stats.dropped++; }
            taskEXIT_CRITICAL();
        }
    }
}
tp_result tp_freertos_event_log_start(tp_freertos_event_log *log,const tp_event_log_io *io,
                                      uint16_t stack_words,UBaseType_t priority) {
    if(!log || !io || !io->read || !io->write || io->size<64 || io->size%32 || !stack_words ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    memset(log,0,sizeof(*log)); log->io=*io; log->queue=xQueueCreate(TP_EVENT_QUEUE_DEPTH,sizeof(tp_event_log_job));
    if(!log->queue) return TP_IO_ERROR;
    log->replies=xQueueCreate(1,sizeof(tp_event_log_reply));
    if(!log->replies) { vQueueDelete(log->queue); log->queue=NULL; return TP_IO_ERROR; }
    if(xTaskCreate(run,"tp-events",stack_words,log,priority,&log->task)!=pdPASS) {
        vQueueDelete(log->replies); vQueueDelete(log->queue); log->replies=NULL; log->queue=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}
tp_result tp_freertos_event_log_submit(tp_freertos_event_log *log,const tp_event *e) {
    if(!log || !log->queue || !e || !e->type) return TP_INVALID;
    tp_event_log_job job={.event=*e}; BaseType_t ok=xQueueSend(log->queue,&job,0);
    taskENTER_CRITICAL(); if(ok==pdPASS) log->stats.queued++; else log->stats.dropped++; taskEXIT_CRITICAL();
    return ok==pdPASS ? TP_OK : TP_BUSY;
}
tp_result tp_freertos_event_log_query(tp_freertos_event_log *log,bool latest,uint32_t sequence) {
    if(!log || !log->queue || !log->replies) return TP_INVALID;
    if(log->query_pending) return TP_BUSY;
    tp_event_log_job job={.query=true,.latest=latest,.sequence=sequence};
    if(xQueueSend(log->queue,&job,0)!=pdPASS) return TP_BUSY;
    log->query_pending=true; return TP_OK;
}
bool tp_freertos_event_log_poll(tp_freertos_event_log *log,tp_result *result,tp_event *event) {
    if(!log || !log->query_pending || !result || !event) return false;
    tp_event_log_reply reply;
    if(xQueueReceive(log->replies,&reply,0)!=pdPASS) return false;
    *result=reply.result; *event=reply.event; log->query_pending=false; return true;
}
void tp_freertos_event_log_stats(tp_freertos_event_log *log,tp_event_log_stats *out) {
    if(!log || !out) return;
    taskENTER_CRITICAL(); *out=log->stats; taskEXIT_CRITICAL();
}
