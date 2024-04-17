/** @file led_display.h
 *  @brief LED display functions
 */

#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H
#include <zephyr/kernel.h>

int write_num_to_display(
    unsigned int display, unsigned int num, const uint32_t *color, int invert_y
);

int turn_display_off(unsigned int display);

#ifdef CONFIG_DEBUG
void led_test_pattern();
#endif
#endif
