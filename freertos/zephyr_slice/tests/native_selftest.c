#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/serial/uart_emul.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>
#include "posix_board_if.h"
#include "../../tests/adc_slice_vector.h"

static uint8_t response[17];
static atomic_t response_length;
static atomic_t response_overflow;

/* Drain synchronously: the UART emulator reports TX complete only when empty. */
static void capture_tx(const struct device *uart, size_t available, void *user_data) {
    ARG_UNUSED(available); ARG_UNUSED(user_data);
    size_t offset = (size_t)atomic_get(&response_length);
    if (offset < sizeof(response)) {
        size_t n = uart_emul_get_tx_data(uart, response + offset, sizeof(response) - offset);
        atomic_set(&response_length, (atomic_val_t)(offset + n));
    } else {
        uint8_t discard[32];
        while (uart_emul_get_tx_data(uart, discard, sizeof(discard)) != 0) {}
        atomic_set(&response_overflow, 1);
    }
}
static uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xffff;
    while (len--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    return crc;
}
static void test_thread(void *a, void *b, void *c) {
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(tp_uart));
    const struct gpio_dt_spec drdy = GPIO_DT_SPEC_GET(DT_ALIAS(tp_adc), drdy_gpios);
    uint8_t request[] = TP_SLICE_REQUEST;
    size_t received = 0;
    k_sleep(K_MSEC(200));
    if (gpio_emul_input_set(drdy.port, drdy.pin, 0) != 0) goto fail;
    k_sleep(K_MSEC(100));
    uart_emul_callback_tx_data_ready_set(uart, capture_tx, NULL);
    if (uart_emul_put_rx_data(uart, request, sizeof(request)) != sizeof(request)) goto fail;
    for (unsigned i = 0; i < 200 && received < sizeof(response); i++) {
        k_sleep(K_MSEC(5));
        received = (size_t)atomic_get(&response_length);
    }
    if (received != sizeof(response) || atomic_get(&response_overflow)) goto fail;
    uint16_t crc = crc16(response, sizeof(response) - 2);
    if (response[0] != 1 || response[1] != 3 || response[2] != 12 ||
        response[3] != 0 || response[4] != 1 || response[5] != 0 || response[6] != 0 ||
        response[7] != 0 || response[8] != 0 || (response[9] == 0 && response[10] == 0) ||
        response[11] != (uint8_t)(TP_SLICE_UV >> 24) || response[12] != (uint8_t)(TP_SLICE_UV >> 16) ||
        response[13] != (uint8_t)(TP_SLICE_UV >> 8) || response[14] != (uint8_t)TP_SLICE_UV ||
        response[15] != (uint8_t)crc || response[16] != (uint8_t)(crc >> 8)) goto fail;
    printk("TP_NATIVE_SELFTEST PASS\n");
    posix_exit(0);
fail:
    printk("TP_NATIVE_SELFTEST FAIL: response length %u\n", (unsigned)received);
    posix_exit(1);
}
K_THREAD_DEFINE(tp_native_selftest, 2048, test_thread, NULL, NULL, NULL, 5, 0, 0);
