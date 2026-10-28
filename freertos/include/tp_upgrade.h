#ifndef TP_UPGRADE_H
#define TP_UPGRADE_H

#include "tp_boot.h"
#include "tp_download.h"

typedef struct {
    const tp_boot_io *boot;
    const tp_download_io *download;
} tp_upgrade;

/* One owner must serialize all boot, download and upgrade operations, including
 * across the checks and I/O below. Never call the lower-level mutating APIs
 * concurrently or outside this coordinator during an upgrade. The manifest
 * must already be authenticated and its bank capacity checked by the caller. */
tp_result tp_upgrade_begin(const tp_upgrade *, const tp_download_manifest *);
tp_result tp_upgrade_resume(const tp_upgrade *);
tp_result tp_upgrade_write(const tp_upgrade *, uint32_t offset, const void *, size_t);
/* Re-verifies the image through both download and boot verification callbacks.
 * Safe to retry after an ambiguous stage error. */
tp_result tp_upgrade_finish(const tp_upgrade *);
#endif
