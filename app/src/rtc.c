#include <rtc.h>

/* Newlib C includes */
#include <stdint.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/posix/time.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

/* pcf85063a driver includes */
#include <drivers/counter/pcf85063a.h>

/* modem includes */
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_key_mgmt.h>

#include <stdlib.h>
#include <stdbool.h>
#include <zephyr/net/socket.h>

#define SNTP_SERVER "pool.ntp.org"
#define SNTP_FALLBACK_SERVER "time.nist.gov"
#define SNTP_INIT_TIMEOUT_MS 3000
#define RETRY_COUNT 3
#define RTC DEVICE_DT_GET(DT_NODELABEL(pcf85063a))

LOG_MODULE_REGISTER(get_rtc, LOG_LEVEL_INF);

const struct device *const rtc = RTC;

static void get_ntp_time(struct sntp_time *ts) {
  int err;

  /* Get sntp time */
  for (int rc = 0; rc < (RETRY_COUNT * 2); rc++) {
    if (rc < RETRY_COUNT) {
      err = sntp_simple(SNTP_SERVER, SNTP_INIT_TIMEOUT_MS, ts);
    } else {
      err = sntp_simple(SNTP_FALLBACK_SERVER, SNTP_INIT_TIMEOUT_MS, ts);
    }

    if (err && (rc == (RETRY_COUNT * 2) - 1)) {
      LOG_ERR("Failed to get time from all NTP pools! Err: %i\n Check your network connection.",
              err);
    } else if (err && (rc == RETRY_COUNT - 1)) {
      LOG_WRN("Unable to get time after 3 tries from NTP pool " SNTP_SERVER
              " . Err: %i\n Attempting to use fallback NTP pool...",
              err);
    } else if (err) {
      LOG_WRN("Failed to get time using SNTP, Err: %i. Retrying...", err);
      k_msleep(100);
    } else {
      break;
    }
  }
}

int set_rtc_time(void) {
  struct sntp_time ts;
  int retry_count = 0;

retry:
  int err = 0;

  if (!device_is_ready(rtc)) {
    LOG_WRN("pcf85063a isn't ready!");
    goto clean_up;
  }

  err = counter_start(rtc);
  if (err < -1) {
    LOG_WRN("Failed to start RTC. Err: %i", err);
    goto clean_up;
  }

  err = lte_lc_init_and_connect();
  if (err < -1) {
    LOG_ERR("LTE failed to connect. Err: %d", err);
    goto clean_up;
  }

  get_ntp_time(&ts);

  /* Convert time to struct tm */
  struct tm rtc_time = *gmtime(&ts.seconds);

	/* Set rtc time */
  err = pcf85063a_set_time(rtc, &rtc_time);
  if (err) {
    LOG_WRN("Failed to set RTC counter. Err: %i", err);
    goto clean_up;
  }

  LOG_INF("RTC time set to: %i:%i:%i %i/%i/%i - %i", rtc_time.tm_hour, rtc_time.tm_min,
          rtc_time.tm_sec, rtc_time.tm_mon + 1, rtc_time.tm_mday, 1900 + rtc_time.tm_year,
          rtc_time.tm_isdst);

clean_up:
  if (retry_count == 0) {
    retry_count++;
    k_msleep(1000);
    goto retry;
  }

  lte_lc_power_off();
  return err;
}

// TODO calculate and offset drift
// Drifted by 1sec between [00:50:35.348,510] and [00:51:05.355,407]

int get_rtc_time(void) {
  struct tm rtc_time = {0};

  /* Get current time from device */
  int err = pcf85063a_get_time(rtc, &rtc_time);
  // int err = counter_get_value_64(rtc, &rtc_time);
  if (err) {
    LOG_ERR("Unable to get time from pcf85063a. Err: %i", err);
    return -1;
  }

  /* Convert to Unix timestamp */
  return mktime(&rtc_time);
}
