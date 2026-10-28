#ifndef TP_W25Q128_H
#define TP_W25Q128_H
#include "tp_core.h"
#define TP_W25Q128_SIZE 0x1000000u
#define TP_W25Q128_PAGE_SIZE 256u
#define TP_W25Q128_SECTOR_SIZE 4096u
/* One call is one CS-low transaction. Always release CS on return; serialize
 * the bus and use a bounded transfer timeout. tx and rx have the same length. */
typedef struct {
    void *ctx;
    tp_result (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t length);
} tp_w25q128_io;
typedef struct {
    tp_w25q128_io io;
    uint32_t started_ms, timeout_ms;
    bool ready, pending;
} tp_w25q128;
/* JEDEC EF 40 18, standard SPI mode; rejects a chip which is already busy.
 * Single owner only. After I/O failure or timeout the instance is invalid:
 * reset/recover the bus and explicitly initialize again once the chip is idle. */
tp_result tp_w25q128_init(tp_w25q128 *, const tp_w25q128_io *);
/* Reads up to one page at any address; no read while an operation is pending. */
tp_result tp_w25q128_read(tp_w25q128 *, uint32_t address, void *out, size_t length);
/* Page program may only clear bits (1 to 0); caller must erase and verify data.
 * A single request must fit within one 256-byte page. */
tp_result tp_w25q128_program(tp_w25q128 *, uint32_t address, const void *data,
                            size_t length, uint32_t now_ms, uint32_t timeout_ms);
/* Sector erase requires a 4 KiB aligned address. */
tp_result tp_w25q128_erase(tp_w25q128 *, uint32_t address, uint32_t now_ms, uint32_t timeout_ms);
/* Returns BUSY while writing; OK only means the chip no longer reports busy,
 * not that its protected bits were modified. Caller must read back and verify. */
tp_result tp_w25q128_poll(tp_w25q128 *, uint32_t now_ms);
#endif
