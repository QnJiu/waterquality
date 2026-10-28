#ifndef TP_PN532_SCAN_H
#define TP_PN532_SCAN_H
#include "tp_pn532.h"

typedef enum { TP_SCAN_START, TP_SCAN_SAM, TP_SCAN_CONFIG, TP_SCAN_POLL,
               TP_SCAN_WAIT, TP_SCAN_FAULT } tp_scan_state;
/* Invoked after every completed scan; card is NULL when no target was found.
 * The callback must copy any needed data, be nonblocking, and not reenter scan. */
typedef void (*tp_scan_report)(void *,const tp_pn532_type_a *card);
typedef struct {
    tp_pn532 device;
    tp_scan_state state;
    tp_scan_report report;
    void *report_ctx;
    uint32_t timeout_ms, interval_ms, last_scan_ms;
    uint8_t retries;
} tp_pn532_scan;
/* Call only after the physical device and transport have been resynchronized. */
tp_result tp_pn532_scan_init(tp_pn532_scan *,const tp_pn532_io *,void *,tp_scan_report,
                             uint32_t command_timeout_ms,uint32_t scan_interval_ms,uint8_t retries);
/* TP_OK or TP_BUSY while operating; other results latch TP_SCAN_FAULT. */
tp_result tp_pn532_scan_tick(tp_pn532_scan *,uint32_t now_ms);
#endif
