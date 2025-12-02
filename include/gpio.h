#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

// GPIO base addresses for different Pi models
#if defined(RPI4)
    #define GPIO_BASE_ADDR 0xFE200000  // BCM2711 (Pi 4)
#elif defined(RPI2) || defined(RPI3)
    #define GPIO_BASE_ADDR 0x3F200000  // BCM2836/BCM2837 (Pi 2, 3)
#else
    #define GPIO_BASE_ADDR 0x20200000  // BCM2835 (Pi 1, Zero)
#endif

#define GPIO_LEN 0xB4

extern volatile uint32_t *gpio;

extern const unsigned int led_lines[8];

void gpio_init(void);
void gpio_cleanup(void);
void gpio_all_off(const unsigned int *lines, int count);
void gpio_set_outputs(const unsigned int *lines, int count);

#endif
