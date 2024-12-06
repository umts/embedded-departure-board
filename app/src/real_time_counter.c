#include "real_time_counter.h"

#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/sys/util.h>

#define RTC DEVICE_DT_GET(DT_ALIAS(rtc))

#define SNTP_SERVER "time.nist.gov"
#define SNTP_FALLBACK_SERVER "us.pool.ntp.org"
#define SNTP_INIT_TIMEOUT_MS 3000
#define RETRY_COUNT 3

LOG_MODULE_REGISTER(real_time_counter);

K_MUTEX_DEFINE(rtc_mutex);

const struct device *const rtc = RTC;

static struct sntp_time ts;

K_SEM_DEFINE(rtc_sync_sem, 0, 1);

static void get_ntp_time(void) {
  int err;

  /* Get sntp time */
  for (int rc = 0; rc < (RETRY_COUNT * 2); rc++) {
    if (rc < RETRY_COUNT) {
      err = sntp_simple(SNTP_SERVER, SNTP_INIT_TIMEOUT_MS, &ts);
    } else {
      err = sntp_simple(SNTP_FALLBACK_SERVER, SNTP_INIT_TIMEOUT_MS, &ts);
    }

    if (err && (rc == (RETRY_COUNT * 2) - 1)) {
      LOG_ERR(
          "Failed to get time from all NTP pools! Err: %i\n Check your network "
          "connection.",
          err
      );
    } else if (err && (rc == RETRY_COUNT - 1)) {
      LOG_WRN(
          "Unable to get time after 3 tries from NTP pool " SNTP_SERVER
          " . Err: %i\n Attempting to use fallback NTP pool...",
          err
      );
    } else if (err) {
      LOG_WRN("Failed to get time using SNTP, Err: %i. Retrying...", err);
      k_msleep(100);
    } else {
      break;
    }
  }
}

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

  (void)get_ntp_time();
  LOG_INF(
      "time since Epoch: high word: %u, low word: %u",
      (uint32_t)(ts.seconds >> 32), (uint32_t)ts.seconds
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

  return ((uint32_t)ts.seconds + (ticks / counter_get_frequency(rtc)));
}
