/* Newlib C includes */
#include <stdint.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/net/sntp.h>
#include <zephyr/posix/time.h>

/* pcf85063a driver includes */
#include <drivers/counter/pcf85063a.h>

/* nrf lib includes */
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#define SNTP_SERVER "time.nist.gov"
#define SNTP_INIT_TIMEOUT_MS 3000
#define RTC DEVICE_DT_GET(DT_NODELABEL(pcf85063a))

LOG_MODULE_REGISTER(get_rtc, LOG_LEVEL_DBG);

const struct device *const rtc = RTC;

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
}

void set_rtc_time(void) {
  int err = 0;
  struct sntp_time ts;

  /* Get sntp time */
  int res = sntp_simple(SNTP_SERVER, SNTP_INIT_TIMEOUT_MS, &ts);

	if (res < 0) {
		LOG_ERR("Cannot set time using SNTP");
	}

  /* Convert time to struct tm */
  struct tm rtc_time = *gmtime(&ts.seconds);

	/* Set rtc time */
	err = pcf85063a_set_time(rtc, &rtc_time);
	if (err) {
		LOG_ERR("Unable to set time. Err: %i", err);
		return;
	}
}

// TODO calculate and offset drift
// Drifted by 1sec between [00:50:35.348,510] and [00:51:05.355,407]

time_t get_rtc_time(void) {
	int err = 0;
  struct tm rtc_time = {0};

	/* Get current time from device */
	err = pcf85063a_get_time(rtc, &rtc_time);
	if (err) {
		LOG_ERR("Unable to get time. Err: %i", err);
	}

	/* Convert to Unix timestamp */
	return mktime(&rtc_time);
}