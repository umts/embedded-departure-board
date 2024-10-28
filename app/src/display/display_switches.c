#include "display_switches.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(display_switches, LOG_LEVEL_INF);

#if !DT_NODE_EXISTS(DT_NODELABEL(display_switch_0))
#error "Overlay for enable switch 0 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(display_switch_1))
#error "Overlay for enable switch 1 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(display_switch_2))
#error "Overlay for enable switch 2 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(display_switch_3))
#error "Overlay for enable switch 3 node not properly defined."
#endif

#if !DT_NODE_EXISTS(DT_NODELABEL(display_switch_4))
#error "Overlay for enable switch 4 node not properly defined."
#endif

static const struct gpio_dt_spec display_switches[] = {
    GPIO_DT_SPEC_GET(DT_NODELABEL(display_switch_0), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(display_switch_1), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(display_switch_2), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(display_switch_3), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(display_switch_4), gpios)
};

int init_display_switches(void) {
  for (size_t i = 0; i < ARRAY_SIZE(display_switches); i++) {
    if (!gpio_is_ready_dt(&display_switches[i])) {
      LOG_ERR(
          "Display switch %d GPIO port %s not ready", i,
          display_switches[i].port->name
      );
      return -ENODEV;
    }

    if (gpio_pin_configure_dt(&display_switches[i], GPIO_OUTPUT_INACTIVE)) {
      LOG_ERR(
          "GPIO pin %d (port %s) failed to configure as GPIO_OUTPUT_INACTIVE",
          display_switches[i].pin, display_switches[i].port->name
      );
      return -1;
    }
  }

  return 0;
}

int display_on(const int display_number) {
  int err = gpio_pin_set_dt(&display_switches[display_number], 1);
  if (err) {
    LOG_ERR("Setting GPIO pin level failed: %d", err);
    return err;
  }
  return 0;
}

int display_off(const int display_number) {
  int err = gpio_pin_set_dt(&display_switches[display_number], 0);
  if (err) {
    LOG_ERR("Setting GPIO pin level failed: %d", err);
    return err;
  }
  return 0;
}
