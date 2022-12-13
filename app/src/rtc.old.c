#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <date_time.h>
#include <zephyr/drivers/counter.h>
#include <drivers/counter/pcf85063a.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

LOG_MODULE_REGISTER(get_rtc, LOG_LEVEL_DBG);

const struct device *rtc;

void start_rtc(void){
  rtc = device_get_binding("PCF85063A");
  if(rtc == NULL) {
    LOG_ERR("Failed to get RTC device Binding\n");
    return;
  }
  counter_start(rtc);
}

static void date_time_event_handler(const struct date_time_evt *evt) {
  switch (evt->type)
  {
  case DATE_TIME_OBTAINED_MODEM:
  case DATE_TIME_OBTAINED_NTP:
  case DATE_TIME_OBTAINED_EXT:
    LOG_INF("DateTime Obtained\n");
    break;
  case DATE_TIME_NOT_OBTAINED:
    LOG_ERR("DateTime Not Obtained\n");
    break;
  default:
    break;
  }
};

int get_rtc(void) {
  int err = date_time_update_async(date_time_event_handler);
  if(err) {
    LOG_ERR("Unable to update time with date_time_update_async. Err: %i\n", err);
  }

  uint64_t ts = 0;
  err = date_time_now(&ts);
  if(err) {
    LOG_ERR("Unable to get DateTime. Err: %i\n", err);
    return 0;
  }

  struct tm *tm_p = gmtime(&ts);
  struct tm time = { 0 };

  memcpy(&time, tm_p, sizeof(struct tm));

  // Set Time
  //pcf85063a_set_time(rtc, &time);

  // Get Time
  return pcf85063a_get_time(rtc, &time);
}