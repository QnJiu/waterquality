#include "ads1220_zephyr.h"

static tp_result transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len) {
    ads1220_zephyr *a = ctx;
    if (!a || !tx || !rx || !len) return TP_INVALID;
    struct spi_buf txbuf = {.buf = (void *)tx, .len = len};
    struct spi_buf rxbuf = {.buf = rx, .len = len};
    struct spi_buf_set txset = {.buffers = &txbuf, .count = 1};
    struct spi_buf_set rxset = {.buffers = &rxbuf, .count = 1};
    return spi_transceive_dt(&a->spi, &txset, &rxset) == 0 ? TP_OK : TP_IO_ERROR;
}
static tp_result data_ready(void *ctx, bool *ready) {
    ads1220_zephyr *a = ctx;
    if (!a || !ready) return TP_INVALID;
    int result = gpio_pin_get_dt(&a->drdy);
    if (result < 0) return TP_IO_ERROR;
    *ready = result != 0;
    return TP_OK;
}
tp_result ads1220_zephyr_init(ads1220_zephyr *a, tp_ads1220_io *io) {
    if (!a || !io) return TP_INVALID;
    if (!spi_is_ready_dt(&a->spi) || !gpio_is_ready_dt(&a->drdy)) return TP_IO_ERROR;
    if (gpio_pin_configure_dt(&a->drdy, GPIO_INPUT) != 0) return TP_IO_ERROR;
    *io = (tp_ads1220_io){a, transfer, data_ready};
    return TP_OK;
}
