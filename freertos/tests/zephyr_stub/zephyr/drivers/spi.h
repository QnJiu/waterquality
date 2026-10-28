#ifndef TEST_ZEPHYR_SPI_H
#define TEST_ZEPHYR_SPI_H
#include <stdbool.h>
#include <stddef.h>
struct spi_dt_spec { int id; };
struct spi_buf { void *buf; size_t len; };
struct spi_buf_set { const struct spi_buf *buffers; size_t count; };
bool spi_is_ready_dt(const struct spi_dt_spec *spec);
int spi_transceive_dt(const struct spi_dt_spec *spec, const struct spi_buf_set *tx,
                      const struct spi_buf_set *rx);
#endif
