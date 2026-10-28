#ifndef TP_W25Q128_IMAGE_H
#define TP_W25Q128_IMAGE_H
#include "tp_w25q128.h"
/* Synchronous adapter for a background task, not the real-time control task.
 * sleep_ms must yield for at least 1 ms and now_ms must be monotonic in ms.
 * Calls, flash and both partitions require one exclusive owner. */
typedef struct {
    tp_w25q128 *flash;
    void *clock_ctx;
    uint32_t (*now_ms)(void *);
    void (*sleep_ms)(void *);
    uint32_t base[2], size, program_timeout_ms, erase_timeout_ms;
    uint8_t protected_bank;
} tp_w25q128_image;
/* Two disjoint, sector-aligned partitions; protected bank cannot be modified. */
tp_result tp_w25q128_image_init(tp_w25q128_image *, tp_w25q128 *,
    void *clock_ctx, uint32_t (*now_ms)(void *), void (*sleep_ms)(void *),
    uint32_t base0,uint32_t base1,uint32_t size,uint8_t protected_bank,
    uint32_t program_timeout_ms,uint32_t erase_timeout_ms);
tp_result tp_w25q128_image_set_protected(tp_w25q128_image *, uint8_t bank);
/* Callback signatures match tp_download_io. Readback verifies program and erase.
 * Repeating identical writes is supported after reset; no erase during write. */
tp_result tp_w25q128_image_read(void *,uint8_t,uint32_t,void *,size_t);
tp_result tp_w25q128_image_write(void *,uint8_t,uint32_t,const void *,size_t);
tp_result tp_w25q128_image_erase(void *,uint8_t);
#endif
