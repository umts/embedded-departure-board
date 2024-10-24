#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/types.h>

#include "display_switches.h"
#include "external_rtc.h"
#include "light_sensor.h"
#include "lte_manager.h"
#include "pwm_leds.h"
#include "update_stop.h"
#include "watchdog_app.h"

#ifdef CONFIG_BOOTLOADER_MCUBOOT
#include "fota.h"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

void log_reset_reason(void) {
  uint32_t cause;
  int err = hwinfo_get_reset_cause(&cause);

  if (err) {
    LOG_ERR("Failed to get reset cause. Err: %d", err);
  } else {
    LOG_INF("Reset Reason: ");
    if (cause == 0) {
      LOG_ERR("RESET_UNKNOWN");
      return;
    }
    if (cause == RESET_PIN) {
      LOG_WRN("RESET_PIN");
    }
    if (cause == RESET_SOFTWARE) {
      LOG_WRN("RESET_SOFTWARE");
    }
    if (cause == RESET_BROWNOUT) {
      LOG_WRN("RESET_BROWNOUT");
    }
    if (cause == RESET_POR) {
      LOG_WRN("RESET_POR");
    }
    if (cause == RESET_WATCHDOG) {
      LOG_WRN("RESET_WATCHDOG");
    }
    if (cause == RESET_DEBUG) {
      LOG_WRN("RESET_DEBUG");
    }
    if (cause == RESET_SECURITY) {
      LOG_WRN("RESET_SECURITY");
    }
    if (cause == RESET_LOW_POWER_WAKE) {
      LOG_WRN("RESET_LOW_POWER_WAKE");
    }
    if (cause == RESET_CPU_LOCKUP) {
      LOG_WRN("RESET_CPU_LOCKUP");
    }
    if (cause == RESET_PARITY) {
      LOG_WRN("RESET_PARITY");
    }
    if (cause == RESET_PLL) {
      LOG_WRN("RESET_PLL");
    }
    if (cause == RESET_CLOCK) {
      LOG_WRN("RESET_CLOCK");
    }
    if (cause == RESET_HARDWARE) {
      LOG_WRN("RESET_HARDWARE");
    }
    if (cause == RESET_USER) {
      LOG_WRN("RESET_USER");
    }
    if (cause == RESET_TEMPERATURE) {
      LOG_WRN("RESET_TEMPERATURE");
    }
    hwinfo_clear_reset_cause();
  }
}

#ifdef CONFIG_LED_DISPLAY_TEST
#include "led_display.h"

int main(void) {
  int err = init_display_switches();
  if (err < 0) {
    LOG_ERR("Failed to initialize display switches. Err: %d", err);
    goto end;
  }

  err = pwm_leds_test();
  if (err) {
    goto end;
  }

  while (1) {
    err = led_test_patern();
    if (err) {
      goto end;
    }

    err = max_power_test();
    if (err) {
      goto end;
    }

    k_msleep(3000);
  }

end:
  LOG_ERR("Reached end of main; waiting for manual reset.");
}

#else
int main(void) {
  int err;
  int lux;
  int wdt_channel_id = -1;

  err = init_display_switches();
  if (err < 0) {
    LOG_ERR("Failed to initialize display switches. Err: %d", err);
    goto reset;
  }

  // Set all displays off because the LEDs have memory
  for (size_t box = 0; box < NUMBER_OF_DISPLAY_BOXES; box++) {
    err = display_off(box);
    if (err < 0) {
      LOG_ERR("Failed to set display switch %d off.", box);
    }
  }

  lux = get_lux();
  if (lux < 0) {
    goto reset;
  }

  err = pwm_leds_set((uint32_t)lux);
  if (err) {
    goto reset;
  }

  (void)log_reset_reason();

#ifdef CONFIG_BOOTLOADER_MCUBOOT
  (void)validate_image();
#endif

  wdt_channel_id = watchdog_init();
  if (wdt_channel_id < 0) {
    LOG_ERR("Failed to initialize watchdog. Err: %d", wdt_channel_id);
    goto reset;
  }

  err = wdt_feed(wdt, wdt_channel_id);
  if (err) {
    LOG_ERR("Failed to feed watchdog. Err: %d", err);
    goto reset;
  }

  err = lte_connect();
  if (err) {
    goto reset;
  }

  if (k_sem_take(&lte_connected_sem, K_FOREVER) == 0) {
    err = set_external_rtc_time();
    if (err) {
      LOG_ERR("Failed to set rtc.");
      goto reset;
    }
  } else {
    LOG_ERR("Failed to take network_connected_sem.");
    goto reset;
  }
  k_sem_give(&lte_connected_sem);

  err = wdt_feed(wdt, wdt_channel_id);
  if (err) {
    LOG_ERR("Failed to feed watchdog. Err: %d", err);
    goto reset;
  }

  // TODO: check for update or wait for update socket
  // (void)download_update();

  (void)k_timer_start(&update_stop_timer, K_SECONDS(30), K_SECONDS(30));
  LOG_INF("update_stop_timer started");

  while (1) {
    if (k_sem_take(&stop_sem, K_NO_WAIT) == 0) {
      err = wdt_feed(wdt, wdt_channel_id);
      if (err) {
        LOG_ERR("Failed to feed watchdog. Err: %d", err);
        goto reset;
      }

      if (update_stop()) {
        goto reset;
      }

      int lux = get_lux();
      if (lux < 0) {
        goto reset;
      }

      err = pwm_leds_set((uint32_t)lux);
      if (err) {
        goto reset;
      }

      err = wdt_feed(wdt, wdt_channel_id);
      if (err) {
        LOG_ERR("Failed to feed watchdog. Err: %d", err);
        goto reset;
      }
    }
    k_cpu_idle();
  }

reset:
  lte_disconnect();

#ifdef CONFIG_DEBUG
  LOG_WRN("Reached end of main; waiting for manual reset.");
  while (1) {
    err = wdt_feed(wdt, wdt_channel_id);
    if (err) {
      LOG_ERR("Failed to feed watchdog. Err: %d", err);
    }
    k_msleep(29000);
  }
#else
  LOG_WRN("Reached end of main; rebooting.");
  /* In ARM implementation sys_reboot ignores the parameter */
  sys_reboot(SYS_REBOOT_COLD);
#endif
}
#endif  // CONFIG_LED_DISPLAY_TEST
