#ifndef TP_STAGED_GUARD_H
#define TP_STAGED_GUARD_H
#include "tp_staged_upgrade.h"
/* All calls, including abort, must be made serially by one task. The lock is
 * held throughout a session to exclude UART upgrades and trial confirmation. */
typedef struct {
    tp_staged_upgrade staged;
    tp_secure_upgrade *upgrade;
    void *ctx;
    tp_result (*maintenance)(void *);
    bool (*lock)(void *);
    void (*unlock)(void *);
    bool active;
} tp_staged_guard;
/* Initialize once before use; never reinitialize an active session. */
tp_result tp_staged_guard_init(tp_staged_guard *,tp_secure_upgrade *,void *,
    tp_result (*maintenance)(void *),bool (*lock)(void *),void (*unlock)(void *));
/* LOCK_BUSY means retry; the maintenance request may complete asynchronously. */
tp_result tp_staged_guard_begin(tp_staged_guard *,const tp_image_hash_io *,const uint8_t *,size_t);
tp_result tp_staged_guard_step(tp_staged_guard *);
tp_result tp_staged_guard_finish(tp_staged_guard *);
/* Stops the session and releases its lock; does not undo already written data. */
void tp_staged_guard_abort(tp_staged_guard *);
#endif
