#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/counter.h>

#include <date_time.h>
#include <drivers/counter/pcf85063a.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <stdint.h>

#define RTC DEVICE_DT_GET(DT_NODELABEL(pcf85063a))

LOG_MODULE_REGISTER(get_rtc, LOG_LEVEL_DBG);

K_SEM_DEFINE(date_time_ready, 0, 1);

const struct device *const rtc = RTC;

static void date_time_event_handler(const struct date_time_evt *evt) {
	switch (evt->type) {
	case DATE_TIME_OBTAINED_MODEM:
  	// LOG_INF("Date & time obtained MODEM.");
		// k_sem_give(&date_time_ready);
		// break;
	case DATE_TIME_OBTAINED_NTP:
  	LOG_INF("Date & time obtained NTP.");
		k_sem_give(&date_time_ready);
		break;
	case DATE_TIME_OBTAINED_EXT:
		LOG_INF("Date & time obtained EXT.");
		k_sem_give(&date_time_ready);
		break;
	case DATE_TIME_NOT_OBTAINED:
		LOG_INF("Date & time not obtained.");
		break;
	default:
		break;
	}
}

void rtc_init() {
  int err = 0;

	/* Check device readiness */
  if (!device_is_ready(rtc)) {
    LOG_ERR("pcf85063a isn't ready!");
  }

	LOG_INF("device is %p, name is %s", rtc, rtc->name);

	err = counter_start(rtc);
	if (err) {
		LOG_ERR("Unable to start RTC. Err: %i", err);
	}

  /* Force time update */
	err = date_time_update_async(date_time_event_handler);
	if (err) {
		LOG_ERR("Unable to update time with date_time_update_async. Err: %i", err);
  }

  	/* Wait for time */
	k_sem_take(&date_time_ready, K_FOREVER);
}

void set_rtc_time(void) {
  int err = 0;
  uint64_t ts = 0;

  /* Get the time */
	err = date_time_now(&ts);
	if (err) {
		LOG_ERR("Unable to get date & time. Err: %i", err);
		return;
	}

  /* Convert time to struct tm */
  struct tm rtc_time = {0};
	struct tm *tm_p = gmtime(&ts);

	if (tm_p != NULL) {
		memcpy(&rtc_time, tm_p, sizeof(struct tm));
	} else {
		LOG_ERR("Unable to convert to broken down UTC");
		return;
	}

	/* Set time */
	err = pcf85063a_set_time(rtc, &rtc_time);
	if (err) {
		LOG_ERR("Unable to set time. Err: %i", err);
		return;
	}
}

// void drift_test(time_t rtc_time) {
//   int err = 0;
//   uint64_t ts = 0;

//   err = date_time_now(&ts);
// 	if (err) {
// 		LOG_ERR("Unable to get date & time. Err: %i", err);
// 	}
// }

// Drifted by 1sec between [00:50:35.348,510] and [00:51:05.355,407]

time_t get_rtc_time(void) {
	int err = 0;
  struct tm rtc_time = {0};

	/* Get current time from device */
	err = pcf85063a_get_time(rtc, &rtc_time);
	if (err) {
		LOG_ERR("Unable to get time. Err: %i", err);
	}

	/* Convert back to timestamp */
	return mktime(&rtc_time);
}