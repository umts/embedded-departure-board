#include "real_time_counter.h"

#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "net/ntp.h"

#define RTC DEVICE_DT_GET(DT_ALIAS(rtc))

LOG_MODULE_REGISTER(real_time_counter);

K_MUTEX_DEFINE(rtc_mutex);

const struct device *const rtc = RTC;

K_SEM_DEFINE(rtc_sync_sem, 0, 1);

static void counter_top_callback(
    const struct device *counter_dev, void *user_data
) {
  LOG_INF("Updating RTC with NTP time");
  (void)k_sem_give(&rtc_sync_sem);
}

int set_rtc_time(void) {
  int err;
  uint32_t freq;
  struct counter_top_cfg top_cfg;

  err = k_mutex_lock(&rtc_mutex, K_MSEC(100));
  if (err) {
    LOG_WRN("Cat set RTC, mutex locked");
    return 1;
  }

  if (!device_is_ready(rtc)) {
    err = 1;
    LOG_WRN("RTC isn't ready!");
    goto clean_up;
  }

  freq = counter_get_frequency(rtc);

  top_cfg.flags = COUNTER_TOP_CFG_RESET_WHEN_LATE;
  // 24 hours = 86400 sec
  top_cfg.ticks = 86400 * freq;
  top_cfg.callback = counter_top_callback;
  top_cfg.user_data = &top_cfg;

  err = counter_set_guard_period(rtc, 30 * freq, 0);
  if (err) {
    LOG_ERR("Failed to set RTC guard value, ERR: %d", err);
  }

  err = counter_set_top_value(rtc, &top_cfg);
  if (err) {
    LOG_ERR("Failed to set RTC top value, ERR: %d", err);
  }

  err = get_ntp_time();
  if (err) {
    LOG_ERR("Failed to get NTP time, ERR: %d", err);
    k_msleep(CONFIG_NTP_REQUEST_TIMEOUT_MS);
    goto clean_up;
  }

  LOG_INF(
      "time since Epoch: high word: %u, low word: %u", (uint32_t)(time_stamp.seconds >> 32),
      (uint32_t)time_stamp.seconds
  );

  err = counter_start(rtc);
  if (err) {
    LOG_WRN("Failed to start RTC. Err: %i", err);
    goto clean_up;
  }

clean_up:
  k_mutex_unlock(&rtc_mutex);
  return err;
}

unsigned int get_rtc_time(void) {
  int err = k_mutex_lock(&rtc_mutex, K_MSEC(100));
  if (err) {
    LOG_WRN("Can't read RTC, mutex locked");
    return -1;
  }

  uint32_t ticks;

  err = counter_get_value(rtc, &ticks);
  if (err < -1) {
    LOG_ERR("Failed to get RTC value. Err: %i", err);
  }

  k_mutex_unlock(&rtc_mutex);

  return ((uint32_t)time_stamp.seconds + (ticks / counter_get_frequency(rtc)));
}
