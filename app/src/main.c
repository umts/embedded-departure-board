/* Newlib C includes */
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

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
#include <routes.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

//#define MY_I2C "I2C_1"
//const struct device *i2c_dev;

struct Routes route_38 = { "38", 20038, {0,0} };
struct Routes route_r29 = { "r29", 10029, {0,0} };
struct Routes route_b43 = { "b43", 30043, {0,0} };
struct Routes route_b43e = { "b43e", 30943, {0,0} };

struct Routes *routes[] = {&route_38, &route_r29, &route_b43, &route_b43e};

// static void lte_init() {
//   int err = 0;

// 	/* Init lte_lc*/
// 	err = lte_lc_init_and_connect();
// 	if (err) {
// 		LOG_ERR("Failed to init and connect. Err: %i", err);
//   }
// }

int minutes_to_departure(struct Routes *route, char direction_code) {
  if ((direction_code == 'N') || (direction_code == 'E')) {
    return (int)(route->etd[0] - get_rtc_time()) / 60;
  } else {
    return (int)(route->etd[1] - get_rtc_time()) / 60;
  }
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
  
  get_departures(http_get_request());

  // LOG_INF("UTC Unix Epoc: %lld", get_rtc_time());
  LOG_INF("R29 North Epoc: %lld", route_r29.etd[0]);

  LOG_INF("R29 South: %d", minutes_to_departure(&route_r29, 'S'));
  LOG_INF("R29 North: %d", minutes_to_departure(&route_r29, 'N'));
  LOG_INF("B43 West: %d", minutes_to_departure(&route_b43, 'W'));
  LOG_INF("B43 East: %d", minutes_to_departure(&route_b43, 'E'));

  // LOG_INF("Route 38 EDT: [North: %lld, South: %lld]", route_38.etd[0], route_38.etd[1]);
  // LOG_INF("Route R29 EDT: [North: %lld, South: %lld]", route_r29.etd[0], route_r29.etd[1]);
  // LOG_INF("Route B43 EDT: [East: %lld, West: %lld]", route_b43.etd[0], route_b43.etd[1]);
  // LOG_INF("Route B43E EDT: [East: %lld, West: %lld]", route_b43e.etd[0], route_b43e.etd[1]);

  lte_lc_power_off();
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