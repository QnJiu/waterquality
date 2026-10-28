#ifndef TEST_ZEPHYR_GPIO_H
#define TEST_ZEPHYR_GPIO_H
#include <stdbool.h>
struct gpio_dt_spec { int id; };
#define GPIO_INPUT 1
bool gpio_is_ready_dt(const struct gpio_dt_spec *spec);
int gpio_pin_configure_dt(const struct gpio_dt_spec *spec, int flags);
int gpio_pin_get_dt(const struct gpio_dt_spec *spec);
#endif
