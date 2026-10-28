#include "tp_m24c02.h"
#include <string.h>
#define SLOT_SIZE 128u
#define COMMIT_OFFSET 73u
static tp_result slot_read(void *ctx, unsigned slot, uint32_t off, void *data, size_t size) {
    if (slot > 1 || off > SLOT_SIZE || size > SLOT_SIZE - off) return TP_INVALID;
    return tp_m24c02_read(ctx, (uint16_t)(slot * SLOT_SIZE + off), data, size);
}
static tp_result slot_write(void *ctx, unsigned slot, uint32_t off, const void *data, size_t size) {
    if (slot > 1 || off > SLOT_SIZE || size > SLOT_SIZE - off) return TP_INVALID;
    return tp_m24c02_write(ctx, (uint16_t)(slot * SLOT_SIZE + off), data, size);
}
static tp_result slot_erase(void *ctx, unsigned slot) {
    /* EEPROM needs no erase: invalidate the old commit before changing data.
     * Read it back so write protection cannot leave an apparently valid slot. */
    const uint8_t invalid[4] = {0, 0, 0, 0}; uint8_t check[4];
    if (slot_write(ctx, slot, COMMIT_OFFSET, invalid, sizeof(invalid)) != TP_OK ||
        slot_read(ctx, slot, COMMIT_OFFSET, check, sizeof(check)) != TP_OK ||
        memcmp(check, invalid, sizeof(check)) != 0) return TP_IO_ERROR;
    return TP_OK;
}
tp_store_io tp_m24c02_store(tp_m24c02 *dev) {
    tp_store_io io = {dev, slot_read, slot_erase, slot_write}; return io;
}
