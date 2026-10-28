/* native_sim-only, deliberately limited to the commands used by this slice. */
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/spi_emul.h>

static uint8_t registers[4];
static int io(const struct emul *emul, const struct spi_config *config,
              const struct spi_buf_set *tx, const struct spi_buf_set *rx) {
    ARG_UNUSED(emul); ARG_UNUSED(config);
    if (!tx || !rx || tx->count != 1 || rx->count != 1 ||
        !tx->buffers[0].buf || !rx->buffers[0].buf ||
        tx->buffers[0].len != rx->buffers[0].len) return -EINVAL;
    const uint8_t *in = tx->buffers[0].buf;
    uint8_t *out = rx->buffers[0].buf;
    size_t n = tx->buffers[0].len;
    if (!n) return -EINVAL;
    memset(out, 0, n);
    if (n == 5 && in[0] == 0x43) memcpy(registers, in + 1, 4);
    else if (n == 5 && in[0] == 0x23) memcpy(out + 1, registers, 4);
    else if (n == 2 && in[0] == 0x40) registers[0] = in[1];
    else if (n == 4 && in[0] == 0x10) { out[1] = 0x10; }
    else if (n != 1 || (in[0] != 0x08 && in[0] != 0x02)) return -EINVAL;
    return 0;
}
static int emul_init(const struct emul *emul, const struct device *parent) {
    ARG_UNUSED(emul); ARG_UNUSED(parent); return 0;
}
static int adc_emul_device_init(const struct device *device) { ARG_UNUSED(device); return 0; }
static const struct spi_emul_api api = {.io = io};
DEVICE_DT_DEFINE(DT_ALIAS(tp_adc), adc_emul_device_init, NULL, NULL, NULL, POST_KERNEL,
                 CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);
EMUL_DT_DEFINE(DT_ALIAS(tp_adc), emul_init, NULL, NULL, &api, NULL);
