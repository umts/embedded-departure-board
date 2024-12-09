#include "watchdog_app.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(watchdog);

#define WDT0 DT_NODELABEL(wdt0)

#if DT_NODE_HAS_STATUS(WDT0, okay)
const struct device *const wdt = DEVICE_DT_GET(WDT0);
#else
#error "wdt0 node is disabled"
#endif

int wdt_channel_id;

int watchdog_init(void) {
  int err;

  if (!device_is_ready(wdt)) {
    LOG_ERR("%s: device not ready.\n", wdt->name);
    return -1;
  }

  struct wdt_timeout_cfg wdt_config = {
      /* Reset SoC when watchdog timer expires */
      .flags = WDT_FLAG_RESET_SOC,

      /* Expire watchdog after max window */
      .window.min = 0U,
      .window.max = (uint32_t)CONFIG_MAX_TIME_INACTIVE_BEFORE_RESET_MS,
      .callback = NULL,
  };

  wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
  if (wdt_channel_id < 0) {
    LOG_ERR("Watchdog install error. Err: %i", wdt_channel_id);
    return wdt_channel_id;
  }

  err = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
  if (err < 0) {
    LOG_ERR("Watchdog setup error. Err: %i", err);
    return err;
  }

  return wdt_channel_id;
}
