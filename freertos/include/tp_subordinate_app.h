#ifndef TP_SUBORDINATE_APP_H
#define TP_SUBORDINATE_APP_H
#include "tp_subordinate_node.h"
#include "tp_door_service.h"

typedef struct {
    tp_sub_node node;
    tp_sub_node_io io;
    tp_door_service *door;
    bool door_fault_is_device_fault;
    tp_result door_result;
} tp_sub_app;

/* door, when supplied, must already be initialized and outlive the app. */
tp_result tp_sub_app_init(tp_sub_app *,const tp_sub_node_io *,uint32_t baud,uint8_t bits,
                          tp_door_service *door,bool door_fault_is_device_fault);
/* Always services both the door and the protocol. door_result reports door status. */
tp_result tp_sub_app_tick(tp_sub_app *,uint32_t us,uint32_t ms);
/* Advisory only: caller must atomically check pending events and hardware idle
 * before sleep, and arrange all required wake sources including a timer. */
bool tp_sub_app_sleep_budget(const tp_sub_app *,uint32_t now_ms,uint32_t maximum_ms,
                             bool transport_idle,uint32_t *budget_ms);
#endif
