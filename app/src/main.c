/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/time.h>
#include <zephyr/types.h>

/* Newlib C includes */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* nrf lib includes */
#include <modem/lte_lc.h>

/* app includes */
#include <custom_http_client.h>
#include <jsmn_parse.h>
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define I2C1 DEVICE_DT_GET(DT_NODELABEL(i2c1))

const struct device *i2c_display = I2C1;

void i2c_display_ready() {
	/* Check device readiness */
  for (int i = 0; i < 3; i++) {
    if (!device_is_ready(i2c_display) && (i == 3 - 1)) {
		  LOG_ERR("I2C device failed to initialize after 3 attempts.");
	  } else if (!device_is_ready(i2c_display)) {
      LOG_WRN("I2C device isn't ready! Retrying...");
      k_msleep(1000);
    } else {
      break;
    }
  }
}

int write_byte_to_display(uint16_t i2c_addr, uint16_t min) {
  i2c_display_ready();
  const uint8_t i2c_tx_buffer[] = { ((min >> 8) & 0xFF), (min & 0xFF) };

  return i2c_write(i2c_display, &i2c_tx_buffer, sizeof(i2c_tx_buffer), i2c_addr);
}

uint16_t minutes_to_departure(Departure *departure) {
  /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
   *  We don't care about the time zone and we just want the seconds.
   *  We strip off the leading '/Date(' and trailing '-400)\' + the last 3 digits to do
   *  a rough conversion to seconds.
   */
  // char edt_string[11];
  // memset(edt_string, '\0', sizeof(edt_string));
  // memcpy(edt_string, ((departure->edt) + 6), 10);
  int edt_ms = departure->etd;  // atoi(edt_string);
  return (uint16_t)(edt_ms - get_rtc_time()) / 60;
}

void main(void) {
  // lte_init();
  rtc_init();
  set_rtc_time();

  static Stop stop = {.last_updated = 0, .id = STOP_ID};

  // TODO: Create seperate thread for time sync
  // while(true) {
  //   k_sleep(K_MINUTES(50));
  //   set_rtc_time();
  // }

  // while(true) {
  //   k_msleep(5000);
  //   LOG_INF("UTC Unix Epoc: %lld", get_rtc_time());
  // }
  while(1) {
    uint16_t min = 0;

    if (http_request_json() != 0) {
      LOG_ERR("HTTP GET request for JSON failed; cleaning up.");
      goto cleanup;
    }

    if (parse_json_for_stop(recv_body_buf, &stop) != 0) {
      LOG_ERR("Failed to parse JSON; cleaning up.");
      goto cleanup;
    }
    // if (parse_json_for_stop(json_test_string) != 0) { goto cleanup; }

    LOG_INF("Stop last updated: %lld", stop.last_updated);
    LOG_INF("Stop ID: %s", stop.id);
    LOG_INF("Stop routes size: %d", stop.routes_size);

    for (int i = 0; i < stop.routes_size; i++) {
      struct RouteDirection route_direction = stop.route_directions[i];
      LOG_INF("========= Route ID: %d, Direction Code: %c, Departures size: %d =========",
              route_direction.id, route_direction.direction_code, route_direction.departures_size);
      for (int j = 0; j < route_direction.departures_size; j++) {
        struct Departure departure = route_direction.departures[j];

        LOG_DBG("Skipped: %s", departure.skipped ? "true" : "false");
        if (!departure.skipped) {
          LOG_DBG("EDT: %d", departure.etd);
          min = minutes_to_departure(&departure);
          LOG_DBG(" - Trip direction: %c", departure.trip.direction_code);
          LOG_DBG(" - Minutes to departure: %d", min);

          if (route_direction.id == 30043 && route_direction.direction_code == 'W') {
            write_byte_to_display(B43_DISPLAY_ADDR, min);
          }
        }
      }
    }
    k_msleep(30000);
  }

cleanup:
  lte_lc_power_off();
  LOG_WRN("Reached end of main; shutting down.");
}
