#ifndef TP_DOOR_SERVICE_H
#define TP_DOOR_SERVICE_H
#include "tp_door.h"
#include "tp_pn532_scan.h"

/* Copies identifier before returning; authentication and response are asynchronous. */
typedef tp_result (*tp_door_request)(void *,uint32_t,const uint8_t *,size_t);
typedef struct {
    tp_pn532_scan scan;
    tp_door door;
    tp_door_request request;
    void *request_ctx;
    uint32_t now;
    tp_result callback_result;
} tp_door_service;

/* Physical PN532 transport must be synchronized before initialization. */
tp_result tp_door_service_init(tp_door_service *,const tp_pn532_io *,const tp_door_io *,
                               void *,tp_door_request,uint32_t,uint32_t,uint8_t,
                               uint32_t,uint32_t,uint32_t);
tp_result tp_door_service_tick(tp_door_service *,uint32_t now);
tp_result tp_door_service_authorize(tp_door_service *,uint32_t now,uint32_t id,bool approved);
#endif
