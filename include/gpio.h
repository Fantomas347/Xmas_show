#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

#define GPIO_BASE_ADDR 0x20200000
#define GPIO_LEN 0xB4

extern volatile uint32_t *gpio;

const unsigned int led_lines[8];

void gpio_init(void);
void gpio_cleanup(void);
void gpio_all_off(const unsigned int *lines, int count);
void gpio_set_outputs(const unsigned int *lines, int count);

#endif
