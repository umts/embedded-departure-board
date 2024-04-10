<<<<<<< HEAD:app/src/external_rtc.c
#include "external_rtc.h"

/* Zephyr includes */
#include <drivers/counter/pcf85063a.h>
=======
#include <app_rtc.h>

/* Zephyr includes */
#include <stdbool.h>
>>>>>>> 2d2bcbb (Use internal RTC):app/src/app_rtc.c
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
<<<<<<< HEAD:app/src/external_rtc.c

#define SNTP_SERVER "time.nist.gov"
#define SNTP_FALLBACK_SERVER "us.pool.ntp.org"
=======
#include <zephyr/net/socket.h>
#include <zephyr/posix/time.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

#if CONFIG_PCF85063A
/* pcf85063a driver includes */
#include <drivers/counter/pcf85063a.h>
#include <zephyr/drivers/gpio.h>

#define RTC DEVICE_DT_GET(DT_NODELABEL(pcf85063a))
#else

#define RTC DEVICE_DT_GET(DT_ALIAS(rtc))
#endif

#define SNTP_SERVER "pool.ntp.org"
#define SNTP_FALLBACK_SERVER "time.nist.gov"
>>>>>>> 2d2bcbb (Use internal RTC):app/src/app_rtc.c
#define SNTP_INIT_TIMEOUT_MS 3000
#define RETRY_COUNT 3

LOG_MODULE_REGISTER(get_rtc);

K_MUTEX_DEFINE(rtc_mutex);

const struct device *const rtc = RTC;

static struct sntp_time ts;

static unsigned int turnover_count = 0;

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

int set_app_rtc_time(void) {
  int err;

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

  err = counter_start(rtc);
  if (err < -1) {
    LOG_WRN("Failed to start RTC. Err: %i", err);
    goto clean_up;
  }

  get_ntp_time();

  /* Convert time to struct tm */
  struct tm rtc_time = *gmtime(&ts.seconds);

#if CONFIG_PCF85063A
  /* Set rtc time */
  err = pcf85063a_set_time(rtc, &rtc_time);
  if (err) {
    LOG_WRN("Failed to set RTC counter. Err: %i", err);
    goto clean_up;
  }
#endif

  LOG_INF(
      "RTC time set to: %i:%i:%i %i/%i/%i - %i", rtc_time.tm_hour,
      rtc_time.tm_min, rtc_time.tm_sec, rtc_time.tm_mon + 1, rtc_time.tm_mday,
      1900 + rtc_time.tm_year, rtc_time.tm_isdst
  );

clean_up:
  k_mutex_unlock(&rtc_mutex);
  return err;
}

int get_app_rtc_time(void) {
  int err = k_mutex_lock(&rtc_mutex, K_MSEC(100));
  if (err) {
    LOG_WRN("Can't set RTC, mutex locked");
    return -1;
  }

/* Get current time from device */
#if CONFIG_PCF85063A
  struct tm rtc_time = {0};
  err = pcf85063a_get_time(rtc, &rtc_time);
#else
  uint32_t ticks;
  err = counter_get_value(rtc, &ticks);
  LOG_WRN("Counter value: %d", ticks);
#endif
  if (err < -1) {
    LOG_ERR("Failed to get RTC value. Err: %i", err);
  }

  k_mutex_unlock(&rtc_mutex);

#if CONFIG_PCF85063A
  /* Convert to Unix timestamp */
  return mktime(rtc_time);
#else
  return (ts.seconds + (ticks / 8));
#endif
}
