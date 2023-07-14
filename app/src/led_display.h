/** @file led_display.h
 *  @brief LED display functions
 */

#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H
#include <sys/_stdint.h>
#include <sys/_types.h>

int write_num_to_display(size_t display, uint8_t brightness, unsigned int num);
#endif
