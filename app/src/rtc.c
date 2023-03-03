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

/* nrf lib includes */
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#define SNTP_SERVER "time.nist.gov"
#define SNTP_INIT_TIMEOUT_MS 3000
#define RETRY_COUNT 3
#define RTC DEVICE_DT_GET(DT_NODELABEL(pcf85063a))

LOG_MODULE_REGISTER(get_rtc, LOG_LEVEL_INF);

const struct device *const rtc = RTC;

void rtc_init() {
	/* Check device readiness */
  for (int i = 0; i < RETRY_COUNT; i++) {
    if (!device_is_ready(rtc) && (i == RETRY_COUNT - 1)) {
		  LOG_ERR("pcf85063a failed to initialize after 3 attempts.");
	  } else if (!device_is_ready(rtc)) {
      LOG_WRN("pcf85063a isn't ready! Retrying...");
      k_msleep(1000);
    } else {
      break;
    }
  }

  /* Start rtc counter */
  for (int i = 0; i < RETRY_COUNT; i++) {
    int err = counter_start(rtc);
    if (err && (i == RETRY_COUNT - 1)) {
		  LOG_ERR("Unable to start RTC after 3 tries. Err: %i", err);
	  } else if (err) {
      LOG_WRN("Failed to start RTC, retrying. Err: %i", err);
    } else {
      break;
    }
  }
}

void set_rtc_time(void) {
  int err;
  struct sntp_time ts;

  /* Get sntp time */
  for (int i = 0; i < RETRY_COUNT; i++) {
    err = sntp_simple(SNTP_SERVER, SNTP_INIT_TIMEOUT_MS, &ts);
    if (err && (i == RETRY_COUNT - 1)) {
		  LOG_ERR("Unable to set time using SNTP after 3 tries. Err: %i", err);
	  } else if (err) {
      LOG_WRN("Failed to set time using SNTP, retrying. Err: %i", err);
    } else {
      break;
    }
  }

  /* Convert time to struct tm */
  struct tm rtc_time = *gmtime(&ts.seconds);

	/* Set rtc time */
  for (int i = 0; i < RETRY_COUNT; i++) {
    err = pcf85063a_set_time(rtc, &rtc_time);
    if (err && (i == RETRY_COUNT - 1)) {
		  LOG_ERR("Unable to set RTC counter after 3 tries. Err: %i", err);
	  } else if (err) {
      LOG_WRN("Failed to set RTC counter, retrying. Err: %i", err);
    } else {
      break;
    }
  }
  LOG_INF("RTC time set to: %i:%i:%i %i/%i/%i - %i", rtc_time.tm_hour, rtc_time.tm_min,
          rtc_time.tm_sec, rtc_time.tm_mon + 1, rtc_time.tm_mday, 1900 + rtc_time.tm_year,
          rtc_time.tm_isdst);
}

// TODO calculate and offset drift
// Drifted by 1sec between [00:50:35.348,510] and [00:51:05.355,407]

uint64_t get_rtc_time(void) {
  // struct tm rtc_time = {0};
  // uint32_t rtc_time;

  // /* Get current time from device */
  // for (int i = 0; i < RETRY_COUNT; i++) {
  //   // int err = pcf85063a_get_time(rtc, &rtc_time);
  //   int err = counter_get_value(rtc, &rtc_time);
  //   if (err && (i == RETRY_COUNT - 1)) {
  // 	  LOG_ERR("Unable to get time from pcf85063a after 3 tries. Err: %i", err);
  //   } else if (err) {
  //     LOG_WRN("Failed to get time from pcf85063a, retrying. Err: %i", err);
  //   } else {
  //     break;
  //   }
  // }

  struct sntp_time ts;

  /* Get sntp time */
  for (int i = 0; i < RETRY_COUNT; i++) {
    int err = sntp_simple(SNTP_SERVER, SNTP_INIT_TIMEOUT_MS, &ts);
    if (err && (i == RETRY_COUNT - 1)) {
      LOG_ERR("Unable to set time using SNTP after 3 tries. Err: %i", err);
    } else if (err) {
      LOG_WRN("Failed to set time using SNTP, retrying. Err: %i", err);
    } else {
      break;
    }
  }

  /* Convert to Unix timestamp */
  // return mktime(&rtc_time);
  // return rtc_time;
  return ts.seconds;
}
