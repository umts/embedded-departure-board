/* Zephyr includes */
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/types.h>

/* app includes */
#include <external_rtc.h>
#include <led_display.h>
#include <lte_manager.h>
#include <update_stop.h>
#include <watchdog_app.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void log_reset_reason(void) {
  uint32_t cause;
  int err = hwinfo_get_reset_cause(&cause);

  if (err) {
    LOG_ERR("Failed to get reset cause. Err: %d", err);
  } else {
    LOG_INF("Reset Reason %d, Flags:", cause);
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

int main(void) {
  int err;
  int wdt_channel_id;

  for (size_t box = 0; box < NUMBER_OF_DISPLAY_BOXES; box++) {
    (void)turn_display_off(box);
  }

  (void)log_reset_reason();

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

  if (k_sem_take(&lte_connected_sem, K_SECONDS(30)) == 0) {
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

  (void)k_timer_start(&update_stop_timer, K_SECONDS(30), K_SECONDS(30));
  LOG_INF("update_stop_timer started");

  // download_update();

  while (1) {
    // led_test_patern();
    if (k_sem_take(&stop_sem, K_NO_WAIT) == 0) {
      err = wdt_feed(wdt, wdt_channel_id);
      if (err) {
        LOG_ERR("Failed to feed watchdog. Err: %d", err);
        goto reset;
      }

      if (update_stop()) {
        goto reset;
      }
    }
    k_cpu_idle();
  }

reset:
  lte_disconnect();
  LOG_WRN("Reached end of main; rebooting.");
  /* In ARM implementation sys_reboot ignores the parameter */
  sys_reboot(SYS_REBOOT_COLD);
}
