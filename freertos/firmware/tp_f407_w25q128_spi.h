#ifndef TP_F407_W25Q128_SPI_H
#define TP_F407_W25Q128_SPI_H
#include "tp_w25q128.h"
#include "FreeRTOS.h"
#include "semphr.h"
/* One instance owns SPI1. Initialize once before the scheduler; no ISR use.
 * Bus acquisition is nonblocking. A transaction has a 10 ms elapsed-cycle
 * deadline, including time preempted by other tasks. */
typedef struct { SemaphoreHandle_t mutex; } tp_f407_w25q128_spi;
tp_result tp_f407_w25q128_spi_init(tp_f407_w25q128_spi *bus);
tp_w25q128_io tp_f407_w25q128_spi_io(tp_f407_w25q128_spi *bus);
#endif
