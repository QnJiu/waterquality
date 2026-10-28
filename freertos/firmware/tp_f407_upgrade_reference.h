#ifndef TP_F407_UPGRADE_REFERENCE_H
#define TP_F407_UPGRADE_REFERENCE_H
#include "tp_f407_upgrade_service.h"

/* Configure options.storage, ctx, healthy, finished and task timing/size before
 * calling. The FRAM must be dedicated/isolated; board code must establish safe
 * outputs and an appropriate watchdog policy during single-bank flash writes.
 * The options must not be modified after start; context, callbacks and bus
 * objects remain valid until reset. Initialize the reference to zero. */
typedef struct {
    tp_f407_upgrade_service_options options;
    tp_f407_upgrade_service service;
    bool locked, started;
} tp_f407_upgrade_reference;
/* Compatible with tp_f407_app_options.start_services; pre-scheduler only.
 * Derives runtime from app and running vector from VTOR. On any error reset
 * before retrying; partially created tasks must not be scheduled. */
tp_result tp_f407_upgrade_reference_start(void *,tp_f407_app *);
/* The same lock must protect all other upgrade metadata and image accesses.
 * Task context only; never use the lock from interrupts. */
bool tp_f407_upgrade_reference_try_lock(void *);
void tp_f407_upgrade_reference_unlock(void *);
#endif
