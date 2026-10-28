#ifndef ADS1220_ZEPHYR_H
#define ADS1220_ZEPHYR_H
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include "tp_ads1220.h"

/* The SPI and GPIO specs must remain valid for the lifetime of this object.
 * DRDY must be declared active-low in devicetree. Use one owner for the ADC. */
typedef struct {
    struct spi_dt_spec spi;
    struct gpio_dt_spec drdy;
} ads1220_zephyr;
tp_result ads1220_zephyr_init(ads1220_zephyr *adapter, tp_ads1220_io *io);
#endif
