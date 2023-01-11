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

/* nrf lib includes */
#include <modem/lte_lc.h>

/* app includes */
// #include <https_client.h>
#include <http_client.h>
#include <parse_json.h>
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

//#define MY_I2C "I2C_1"
//const struct device *i2c_dev;

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
        LOG_INF("  - %s: %d", departure.isd, minutes_to_departure(&departure));
      }
      free(&departure);
    }
    free(&route);
  }
  free(&stop);
  lte_lc_power_off();
  LOG_WRN("FIN");
}

  //printk("\n%s", http_get_request());
  
  // // Bind i2c
	// i2c_dev = device_get_binding(MY_I2C);
	// if (i2c_dev == NULL) {
	// 	printk("Can't bind I2C device %s\n", MY_I2C);
	// 	return;
	// }

	// // Write to i2c
	// uint8_t i2c_addr = 0x41;
  // unsigned char i2c_tx_buffer[] = {'3', '0'};

  // i2c_write(i2c_dev, &i2c_tx_buffer, sizeof(i2c_tx_buffer), i2c_addr);