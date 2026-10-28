#ifndef TP_EVENT_LOG_H
#define TP_EVENT_LOG_H
#include "tp_core.h"
#define TP_EVENT_LOG_RECORD_SIZE 32u
/* The backend must write individual bytes independently; calls must complete or
 * report failure. Region size is an exact multiple of 32 and at least 64 bytes.
 * Calls may block: use only from a storage worker, never the control task. */
typedef struct {
    void *ctx;
    tp_result (*read)(void *, uint32_t offset, void *, size_t);
    tp_result (*write)(void *, uint32_t offset, const void *, size_t);
    uint32_t size;
} tp_event_log_io;
typedef struct {
    uint32_t sequence;
    int64_t utc_seconds;
    uint32_t value;
    uint16_t type, detail;
    bool utc_valid;
} tp_event;
typedef struct {
    tp_event_log_io io;
    uint32_t slots, latest_slot, latest_sequence;
    bool open, has_records;
} tp_event_log;
/* Scan all slots; corrupt/incomplete slots are ignored, I/O failure is returned.
 * One owner must serialize open, append and read. */
tp_result tp_event_log_open(tp_event_log *, const tp_event_log_io *);
/* Assigns the sequence on success. After any I/O failure reopen before use;
 * a failed commit may nevertheless have reached the device. */
tp_result tp_event_log_append(tp_event_log *, tp_event *);
/* Read by sequence. TP_INVALID means the record is absent or overwritten. */
tp_result tp_event_log_read(tp_event_log *, uint32_t sequence, tp_event *);
#endif
