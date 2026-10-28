#ifndef TP_F407_ADS1220_SPI_H
#define TP_F407_ADS1220_SPI_H
#include "tp_ads1220.h"
#include "FreeRTOS.h"
#include "semphr.h"
/* One instance owns SPI2. Initialize once before starting the scheduler.
 * No ISR use. Bus acquisition is nonblocking; transfers are bounded by 1 ms
 * of core cycles, excluding time spent preempted by higher priority work. */
typedef struct { SemaphoreHandle_t mutex; } tp_f407_ads1220_spi;
tp_result tp_f407_ads1220_spi_init(tp_f407_ads1220_spi *bus);
tp_ads1220_io tp_f407_ads1220_spi_io(tp_f407_ads1220_spi *bus);
#endif
