#include "ads1220_zephyr.h"
#include <assert.h>
#include <string.h>

static bool spi_ready = true, gpio_ready = true;
static int configured, pin_value, spi_error;
static uint8_t regs[4];
bool spi_is_ready_dt(const struct spi_dt_spec *s) { return s->id == 1 && spi_ready; }
bool gpio_is_ready_dt(const struct gpio_dt_spec *s) { return s->id == 2 && gpio_ready; }
int gpio_pin_configure_dt(const struct gpio_dt_spec *s, int flags) {
    assert(s->id == 2 && flags == GPIO_INPUT); return configured;
}
int gpio_pin_get_dt(const struct gpio_dt_spec *s) { assert(s->id == 2); return pin_value; }
int spi_transceive_dt(const struct spi_dt_spec *s, const struct spi_buf_set *t, const struct spi_buf_set *r) {
    assert(s->id == 1 && t->count == 1 && r->count == 1);
    assert(t->buffers[0].len == r->buffers[0].len);
    if (spi_error) return -1;
    const uint8_t *tx = t->buffers[0].buf; uint8_t *rx = r->buffers[0].buf;
    size_t n = t->buffers[0].len; memset(rx, 0, n);
    if (n == 5 && tx[0] == 0x43) memcpy(regs, tx + 1, 4);
    else if (n == 5 && tx[0] == 0x23) memcpy(rx + 1, regs, 4);
    else if (n == 2 && tx[0] == 0x40) regs[0] = tx[1];
    else if (n == 4 && tx[0] == 0x10) rx[3] = 42;
    else assert(n == 1 && tx[0] == 0x08);
    return 0;
}
int main(void) {
    ads1220_zephyr adapter = {{1}, {2}}; tp_ads1220_io io; tp_ads1220 adc;
    uint8_t config[4] = {1,0,0,0}; int32_t raw;
    spi_ready = false; assert(ads1220_zephyr_init(&adapter, &io) == TP_IO_ERROR); spi_ready = true;
    gpio_ready = false; assert(ads1220_zephyr_init(&adapter, &io) == TP_IO_ERROR); gpio_ready = true;
    configured = -1; assert(ads1220_zephyr_init(&adapter, &io) == TP_IO_ERROR); configured = 0;
    assert(ads1220_zephyr_init(&adapter, &io) == TP_OK);
    assert(tp_ads1220_init(&adc, &io, config, 50) == TP_OK);
    assert(tp_ads1220_start(&adc, 8, 0) == TP_OK);
    pin_value = 0; assert(tp_ads1220_poll(&adc, 1, &raw) == TP_BUSY);
    pin_value = 1; assert(tp_ads1220_poll(&adc, 2, &raw) == TP_OK && raw == 42);
    assert(tp_ads1220_start(&adc, 8, 3) == TP_OK);
    pin_value = -1; assert(tp_ads1220_poll(&adc, 4, &raw) == TP_IO_ERROR);
    spi_error = 1; assert(tp_ads1220_init(&adc, &io, config, 50) == TP_IO_ERROR);
    return 0;
}
