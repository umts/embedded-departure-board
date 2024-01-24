#include "sys_init.h"

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sys_init, LOG_LEVEL_DBG);

K_SEM_DEFINE(my_sem, 0, 1);

void log_reset_reason(void) {
  uint32_t cause;
  int err = hwinfo_get_reset_cause(&cause);

  if (err) {
    LOG_ERR("Failed to get reset cause. Err: %d", err);
  } else {
    LOG_DBG("Reset Reason %d, Flags:", cause);
    if (cause == 0) {
      LOG_ERR("RESET_UNKNOWN");
      return;
    }
    if (cause == RESET_PIN) {
      LOG_DBG("RESET_PIN");
    }
    if (cause == RESET_SOFTWARE) {
      LOG_DBG("RESET_SOFTWARE");
    }
    if (cause == RESET_BROWNOUT) {
      LOG_DBG("RESET_BROWNOUT");
    }
    if (cause == RESET_POR) {
      LOG_DBG("RESET_POR");
    }
    if (cause == RESET_WATCHDOG) {
      LOG_DBG("RESET_WATCHDOG");
    }
    if (cause == RESET_DEBUG) {
      LOG_DBG("RESET_DEBUG");
    }
    if (cause == RESET_SECURITY) {
      LOG_DBG("RESET_SECURITY");
    }
    if (cause == RESET_LOW_POWER_WAKE) {
      LOG_DBG("RESET_LOW_POWER_WAKE");
    }
    if (cause == RESET_CPU_LOCKUP) {
      LOG_DBG("RESET_CPU_LOCKUP");
    }
    if (cause == RESET_PARITY) {
      LOG_DBG("RESET_PARITY");
    }
    if (cause == RESET_PLL) {
      LOG_DBG("RESET_PLL");
    }
    if (cause == RESET_CLOCK) {
      LOG_DBG("RESET_CLOCK");
    }
    if (cause == RESET_HARDWARE) {
      LOG_DBG("RESET_HARDWARE");
    }
    if (cause == RESET_USER) {
      LOG_DBG("RESET_USER");
    }
    if (cause == RESET_TEMPERATURE) {
      LOG_DBG("RESET_TEMPERATURE");
    }
    hwinfo_clear_reset_cause();
  }
}
