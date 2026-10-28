#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include "ads1220_zephyr.h"
#include "tp_adc_status.h"

LOG_MODULE_REGISTER(tp_slice, LOG_LEVEL_INF);
#define ADC_NODE DT_ALIAS(tp_adc)
#define MODBUS_NODE DT_ALIAS(tp_modbus)

static ads1220_zephyr bus = {
    .spi = SPI_DT_SPEC_GET(ADC_NODE, SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPHA, 0),
    .drdy = GPIO_DT_SPEC_GET(ADC_NODE, drdy_gpios),
};
static tp_ads1220 adc;
static tp_adc_reader reader;
static tp_adc_status status;
static tp_ads1220_io io;
static const uint8_t registers[4] = {0x01, 0, 0, 0};
/* Internal 2.048 V reference, gain 1, 24-bit bipolar output. */
static const tp_adc_channel channel = {8, 2.048f / 8388608.0f};
K_MUTEX_DEFINE(status_mutex);

static int holding_read(uint16_t addr, uint16_t *value) {
    k_mutex_lock(&status_mutex, K_FOREVER);
    tp_result result = tp_adc_status_read(&status, addr, 1, value);
    k_mutex_unlock(&status_mutex);
    return result == TP_OK ? 0 : -ENOTSUP;
}
static struct modbus_user_callbacks callbacks = {.holding_reg_rd = holding_read};
static struct modbus_iface_param params = {
    .mode = MODBUS_MODE_RTU,
    .server = {.user_cb = &callbacks, .unit_id = 1},
    .serial = {.baud = 19200, .parity = UART_CFG_PARITY_EVEN},
};
static int start_server(void) {
    int iface = modbus_iface_get_by_name(DEVICE_DT_NAME(MODBUS_NODE));
    return iface < 0 ? iface : modbus_init_server(iface, params);
}

int main(void) {
    bool initialized = false;
    if (start_server() != 0) {
        LOG_ERR("Modbus initialization failed");
        return 1;
    }
    for (;;) {
        if (!initialized || !adc.configured) {
            tp_result r = ads1220_zephyr_init(&bus, &io);
            if (r == TP_OK) r = tp_ads1220_init(&adc, &io, registers, 100);
            if (r == TP_OK && !initialized) {
                r = tp_adc_reader_init(&reader, &adc, &channel, 1);
                if (r == TP_OK) {
                    k_mutex_lock(&status_mutex, K_FOREVER);
                    r = tp_adc_status_init(&status, &reader, 0, 1000, k_uptime_get_32());
                    k_mutex_unlock(&status_mutex);
                }
                initialized = r == TP_OK;
            }
            if (r != TP_OK) {
                k_mutex_lock(&status_mutex, K_FOREVER);
                status.status = 2; status.result = r; status.microvolts = 0;
                k_mutex_unlock(&status_mutex);
                k_sleep(K_MSEC(1000));
                continue;
            }
        }
        k_mutex_lock(&status_mutex, K_FOREVER);
        tp_adc_status_poll(&status, k_uptime_get_32());
        k_mutex_unlock(&status_mutex);
        k_sleep(K_MSEC(5));
    }
}
