#include "tp_freertos_runtime.h"
#include <string.h>
#if configTICK_RATE_HZ != 1000
#error "tp_freertos_runtime requires a 1 kHz FreeRTOS tick"
#endif
_Static_assert(sizeof(TickType_t) >= 4, "32-bit FreeRTOS ticks required");

static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffffu;
    while (n--) { crc ^= *p++; for (unsigned i = 0; i < 8; i++)
        crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) ? 0xa001u : 0u)); }
    return crc;
}
/* Promote only an addressed, CRC-valid stop command. */
static bool is_stop(const tp_freertos_runtime *rt, const tp_runtime_frame *f) {
    const uint8_t *b = f->bytes; size_t n = f->length;
    if (n < 8 || (b[0] != 0 && b[0] != rt->ports[f->port].address) ||
        crc16(b,n-2) != (uint16_t)(b[n-2] | ((uint16_t)b[n-1] << 8))) return false;
    if (b[2] || b[3]) return false;
    return (n == 8 && b[1] == 6 && b[4] == 0 && b[5] == 2) ||
           (n == 11 && b[1] == 16 && b[4] == 0 && b[5] == 1 &&
            b[6] == 2 && b[7] == 0 && b[8] == 2);
}
static void process(tp_freertos_runtime *rt, const tp_runtime_frame *f) {
    uint8_t response[TP_RUNTIME_FRAME_MAX]; size_t length = 0;
    tp_result r = tp_modbus_frame(&rt->ports[f->port], f->bytes, f->length,
                                  response, sizeof(response), &length);
    if (r == TP_OK && length && rt->io.send(rt->io.ctx, f->port, response, length) != TP_OK &&
        rt->io.communication_error) rt->io.communication_error(rt->io.ctx, f->port);
}
static tp_result enqueue_save(void *ctx, const tp_config *config) {
    tp_freertos_runtime *rt=ctx;
    if (!rt->storage_task || xQueueSend(rt->save_requests,config,0)!=pdPASS) return TP_BUSY;
    return TP_OK;
}
static void storage_task(void *arg) {
    tp_freertos_runtime *rt=arg; tp_config config;
    for (;;) {
        if (xQueueReceive(rt->save_requests,&config,portMAX_DELAY)==pdPASS) {
            tp_result result=tp_store_save(&rt->storage,&config);
            (void)xQueueSend(rt->save_results,&result,portMAX_DELAY);
            if (rt->owner) xTaskNotifyGive(rt->owner);
        }
    }
}
tp_result tp_runtime_attach_storage(tp_freertos_runtime *rt, const tp_store_io *store, const tp_config *cfg) {
    if (!rt || !rt->requests || rt->owner || rt->save_requests || !store || !store->read ||
        !store->write || !store->erase || tp_config_validate(cfg)!=TP_OK) return TP_INVALID;
    rt->save_requests=xQueueCreate(1,sizeof(tp_config));
    if (!rt->save_requests) return TP_IO_ERROR;
    rt->save_results=xQueueCreate(1,sizeof(tp_result));
    if (!rt->save_results) { vQueueDelete(rt->save_requests); rt->save_requests=NULL; return TP_IO_ERROR; }
    rt->storage=*store;
    tp_result r=tp_service_attach_config(rt->service,cfg,rt,enqueue_save);
    if (r!=TP_OK) {
        vQueueDelete(rt->save_requests); vQueueDelete(rt->save_results);
        rt->save_requests=NULL; rt->save_results=NULL;
    }
    return r;
}
tp_result tp_runtime_bootstrap_storage(tp_freertos_runtime *rt, const tp_store_io *store,
    const tp_config *defaults, uint32_t *sequence, tp_config_source *source) {
    if (!rt || !sequence || !source || !store || !store->read || !store->write || !store->erase)
        return TP_INVALID;
    tp_config cfg; uint32_t seq; tp_config_source origin;
    tp_result r = tp_store_bootstrap(store, defaults, &cfg, &seq, &origin);
    if (r != TP_OK) return r;
    r = tp_runtime_attach_storage(rt, store, &cfg);
    if (r == TP_OK) { *sequence = seq; *source = origin; }
    return r;
}
static void task(void *arg) {
    tp_freertos_runtime *rt = arg;
    const TickType_t period = pdMS_TO_TICKS(rt->tick_ms) ? pdMS_TO_TICKS(rt->tick_ms) : 1;
    for (;;) {
        tp_runtime_frame f; unsigned budget = TP_RUNTIME_QUEUE_DEPTH;
        /* A stop overtakes pending traffic. Discard earlier pending requests so a
         * queued start cannot restart the instrument immediately after the stop. */
        while (xQueueReceive(rt->stops, &f, 0) == pdPASS) {
            (void)xQueueReset(rt->requests);
            rt->service->now_ms = (uint32_t)xTaskGetTickCount();
            process(rt, &f);
        }
        tp_result save_result;
        if (rt->save_results && xQueueReceive(rt->save_results,&save_result,0)==pdPASS)
            tp_service_save_complete(rt->service,save_result);
        int64_t utc = 0;
        bool valid = rt->io.rtc_utc && rt->io.rtc_utc(rt->io.ctx, &utc);
        tp_service_tick(rt->service, (uint32_t)xTaskGetTickCount(), valid, utc);
        while (budget-- && xQueueReceive(rt->requests, &f, 0) == pdPASS) {
            /* Check again between requests to bound stop latency. */
            if (uxQueueMessagesWaiting(rt->stops)) {
                (void)xQueueReset(rt->requests);
                break;
            }
            rt->service->now_ms = (uint32_t)xTaskGetTickCount();
            process(rt, &f);
        }
        /* Notification also wakes the task when a stop arrives. */
        (void)ulTaskNotifyTake(pdTRUE, period);
    }
}
tp_result tp_runtime_init(tp_freertos_runtime *rt, tp_service *service,
    const tp_runtime_io *io, const uint8_t *addresses, size_t count, uint32_t tick_ms) {
    if (!rt || !service || !io || !io->send || !addresses || !count ||
        count > TP_RUNTIME_PORTS || !tick_ms || tick_ms > 1000) return TP_INVALID;
    for (size_t i=0; i<count; i++) if (!addresses[i] || addresses[i]>247) return TP_INVALID;
    memset(rt, 0, sizeof(*rt)); rt->service = service; rt->io = *io;
    rt->port_count = count; rt->tick_ms = tick_ms;
    for (size_t i=0; i<count; i++) {
        rt->ports[i].ctx = service; rt->ports[i].address = addresses[i];
        rt->ports[i].read_holding = tp_service_read_holding;
        rt->ports[i].write_holding = tp_service_write_holding;
    }
    rt->requests = xQueueCreate(TP_RUNTIME_QUEUE_DEPTH, sizeof(tp_runtime_frame));
    if (!rt->requests) return TP_IO_ERROR;
    rt->stops = xQueueCreate(TP_RUNTIME_STOP_DEPTH, sizeof(tp_runtime_frame));
    if (!rt->stops) { vQueueDelete(rt->requests); rt->requests = NULL; return TP_IO_ERROR; }
    return TP_OK;
}
tp_result tp_runtime_start(tp_freertos_runtime *rt, uint16_t stack_words, UBaseType_t priority) {
    if (!rt || !rt->requests || !rt->stops || rt->owner || !stack_words) return TP_INVALID;
    if (rt->save_requests && xTaskCreate(storage_task, "tp-storage", stack_words, rt,
                                         priority > 0 ? priority - 1 : 0, &rt->storage_task) != pdPASS)
        return TP_IO_ERROR;
    if (xTaskCreate(task, "tp-control", stack_words, rt, priority, &rt->owner) != pdPASS) {
        if (rt->storage_task) { vTaskDelete(rt->storage_task); rt->storage_task=NULL; }
        return TP_IO_ERROR;
    }
    return TP_OK;
}
tp_result tp_runtime_submit(tp_freertos_runtime *rt, unsigned port, const uint8_t *bytes, size_t len) {
    if (!rt || !rt->requests || !rt->stops || !rt->owner || !bytes ||
        port >= rt->port_count || len < 4 || len > TP_RUNTIME_FRAME_MAX) return TP_INVALID;
    tp_runtime_frame f; f.port = (uint8_t)port; f.length = (uint16_t)len; memcpy(f.bytes,bytes,len);
    if (xQueueSend(is_stop(rt,&f) ? rt->stops : rt->requests, &f, 0) != pdPASS) {
        if (rt->io.communication_error) rt->io.communication_error(rt->io.ctx,port);
        return TP_BUSY;
    }
    xTaskNotifyGive(rt->owner); return TP_OK;
}



static tp_result rtu_submit(void *ctx, unsigned port, const uint8_t *frame, size_t len) {
    return tp_runtime_submit((tp_freertos_runtime *)ctx,port,frame,len);
}
static void rtu_error(void *ctx, unsigned port, tp_result result) {
    tp_freertos_runtime *rt=ctx;
    /* Queue-full is already reported by tp_runtime_submit. */
    if (result != TP_BUSY && rt->io.communication_error) rt->io.communication_error(rt->io.ctx,port);
}
tp_result tp_runtime_rtu_port_init(tp_freertos_runtime *rt, tp_rtu_port *port,
    unsigned number, uint32_t baud, uint8_t bits) {
    if (!rt || !port || !rt->requests || number >= rt->port_count) return TP_INVALID;
    return tp_rtu_port_init(port,number,baud,bits,rt,rtu_submit,rtu_error);
}
