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

#ifdef CONFIG_LED_DISPLAY_TEST
int led_test_patern(void);
#endif  // CONFIG_LED_DISPLAY_TEST
#endif  // LED_DISPLAY_H
