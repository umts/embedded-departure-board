/* Newlib C includes */
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/time.h>
#include <zephyr/drivers/i2c.h>

/* nrf lib includes */
#include <modem/lte_lc.h>

/* app includes */
// #include <https_client.h>
#include <http_client.h>
#include <parse_json.h>
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define DISPLAY_ADDR 0x41
#define I2C1 DEVICE_DT_GET(DT_NODELABEL(i2c1))

const struct device *i2c1 = I2C1;

void i2c_init() {
	/* Check device readiness */
  for (int i = 0; i < 3; i++) {
    if (!device_is_ready(i2c1) && (i == 3 - 1)) {
		  LOG_ERR("I2C device failed to initialize after 3 attempts.");
	  } else if (!device_is_ready(i2c1)) {
      LOG_WRN("I2C device isn't ready! Retrying...");
      k_msleep(1000);
    } else {
      break;
    }
  }
}

int write_byte_to_display(uint16_t i2c_addr, int min) {
  if (min > 255) {
    LOG_ERR("Minutes to departure is too larg (>255)");
    return 1;
  }
  unsigned char i2c_tx_buffer = (unsigned char)min;

  return i2c_write(i2c1, &i2c_tx_buffer, 1, i2c_addr);
}

int minutes_to_departure(struct Departure *departure) {
  return (int)(departure->etd - get_rtc_time()) / 60;
}

void main(void) {
  //lte_init();
  rtc_init();
  set_rtc_time();
  
  // TODO: Create seperate thread for time sync
  // while(true) {
  //   k_sleep(K_MINUTES(50));
  //   set_rtc_time();
  // }

  // while(true) {
  //   k_msleep(5000);
  //   LOG_INF("UTC Unix Epoc: %lld", get_rtc_time());
  // }
  
  struct Stop *stop = parse_stop_json(http_get_request());
  k_msleep(1000);

  for (int i = 0; i < stop->routes_size; i++) {
    struct Route *route = stop->routes[i];
    LOG_INF(
      "================ Route ID: %d, Direction Code: %c ================",
      route->id, route->direction
    );
    for (int j = 0; j < route->departures_size; j++) {
      struct Departure departure = route->departures[j];
      if (!departure.skipped) {
        int min = minutes_to_departure(&departure);
        LOG_INF("  - %s: %d", departure.isd, min);
        write_byte_to_display(DISPLAY_ADDR, min);
      }
      free(&departure);
    }
    free(&route);
  }
  free(&stop);
  lte_lc_power_off();
  LOG_WRN("FIN");
}