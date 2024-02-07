#include "watchdog_app.h"

#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(watchdog, LOG_LEVEL_INF);

#define WDT DEVICE_DT_GET(DT_NODELABEL(wdt0))
/* Nordic supports a callback, but it has 61.2 us to complete before
 * the reset occurs, which is too short for this sample to do anything
 * useful. Explicitly disallow use of the callback.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_wdt)
#define WDT_ALLOW_CALLBACK 0
#else
#define WDT_ALLOW_CALLBACK 1
#endif
#define WDT_MAX_WINDOW 60000U
#define WDT_MIN_WINDOW 0U
#define WDG_FEED_INTERVAL 50U
#define WDT_OPT WDT_OPT_PAUSE_HALTED_BY_DBG

#define WDT0_NODE DT_NODELABEL(wdt0)

#ifndef WDT_MAX_WINDOW
#define WDT_MAX_WINDOW 2000U
#endif

#ifndef WDT_MIN_WINDOW
#define WDT_MIN_WINDOW 0U
#endif

#if DT_NODE_HAS_STATUS(WDT0_NODE, okay)
const struct device *const wdt = DEVICE_DT_GET(WDT0_NODE);
#else
#error "Node is disabled"
#endif

int watchdog_init(void) {
  int err;
  int wdt_channel_id;

  if (!device_is_ready(wdt)) {
    LOG_ERR("%s: device not ready.\n", wdt->name);
    return -1;
  }

  struct wdt_timeout_cfg wdt_config = {
      /* Reset SoC when watchdog timer expires. */
      .flags = WDT_FLAG_RESET_SOC,

      /* Expire watchdog after max window */
      .window.min = WDT_MIN_WINDOW,
      .window.max = WDT_MAX_WINDOW,
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
