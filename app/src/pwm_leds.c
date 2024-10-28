#include "pwm_leds.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm_leds, LOG_LEVEL_INF);

static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));

#define MAX_PERIOD 250000000
#define TRIGGER_LUX 0xA

int pwm_leds_set(uint32_t value) {
  uint32_t pulse;
  int ret;

  if (value < TRIGGER_LUX) {
    pulse = MAX_PERIOD;
  } else {
    pulse = 0;
  }

  if (!pwm_is_ready_dt(&pwm_led0)) {
    LOG_ERR("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
    return 1;
  }

  ret = pwm_set_dt(&pwm_led0, MAX_PERIOD, pulse);
  if (ret) {
    LOG_ERR("Error %d: failed to set pulse width", ret);
    return 1;
  }

  LOG_INF("PWM pulse set to %u", pulse);

  return 0;
}

#ifdef CONFIG_LED_DISPLAY_TEST
int pwm_leds_test(void) {
  uint32_t max_period;

  LOG_INF("PWM-based blinky");

  if (!pwm_is_ready_dt(&pwm_led0)) {
    LOG_ERR("Error: PWM device %s is not ready", pwm_led0.dev->name);
    return 1;
  }

  LOG_INF("Calibrating for channel %d...", pwm_led0.channel);
  max_period = PWM_SEC(1U);
  while (pwm_set_dt(&pwm_led0, max_period, max_period / 2U)) {
    max_period /= 2U;
    if (max_period < (4U * (PWM_SEC(1U) / 128U))) {
      LOG_ERR(
          "Error: PWM device "
          "does not support a period at least %lu",
          4U * (PWM_SEC(1U) / 128U)
      );
      return 1;
    }
  }

  LOG_INF(
      "Done calibrating; maximum/minimum periods %u/%lu nsec", max_period,
      PWM_SEC(1U) / 128U
  );

  return 0;
}
#endif  // CONFIG_LED_DISPLAY_TEST
