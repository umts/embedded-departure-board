/** @brief PWM LED functions
 */

#ifndef PWM_LEDS_H
#define PWM_LEDS_H
#include <zephyr/types.h>

int pwm_leds_set(uint32_t value);
#ifdef CONFIG_LED_DISPLAY_TEST
int pwm_leds_test(void);
#endif  // CONFIG_LED_DISPLAY_TEST
#endif  // PWM_LEDS_H
