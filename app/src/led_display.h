/** @file led_display.h
 *  @brief LED display functions
 */

#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H
#include <zephyr/types.h>

#include "update_stop.h"

int write_num_to_display(
    DisplayBox *display, uint8_t brightness, unsigned int num
);

int turn_display_off(size_t display_position);

#ifdef CONFIG_DEBUG
void led_test_patern(void);
#endif
#endif
