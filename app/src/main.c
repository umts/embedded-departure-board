/* Zephyr includes */
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/types.h>

/* nrf lib includes */
#include <modem/nrf_modem_lib.h>

/* app includes */
#include <custom_http_client.h>
#include <jsmn_parse.h>
#include <led_display.h>
#include <rtc.h>
#include <stop.h>
#include <stop_display_index.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static unsigned int minutes_to_departure(Departure *departure) {
  int edt_ms = departure->etd;
  return (unsigned int)(edt_ms - get_rtc_time()) / 60;
}

static void log_reset_reason(void) {
  uint32_t cause;
  int err = hwinfo_get_reset_cause(&cause);

  if (err) {
    LOG_ERR("Failed to get reset cause. Err: %d", err);
  } else {
    LOG_DBG("Reset Reason %d, Flags:", cause);
    if (cause == 0) {
      LOG_ERR("RESET_UNKNOWN");
      return;
    }
    if (cause == RESET_PIN) {
      LOG_DBG("RESET_PIN");
    }
    if (cause == RESET_SOFTWARE) {
      LOG_DBG("RESET_SOFTWARE");
    }
    if (cause == RESET_BROWNOUT) {
      LOG_DBG("RESET_BROWNOUT");
    }
    if (cause == RESET_POR) {
      LOG_DBG("RESET_POR");
    }
    if (cause == RESET_WATCHDOG) {
      LOG_DBG("RESET_WATCHDOG");
    }
    if (cause == RESET_DEBUG) {
      LOG_DBG("RESET_DEBUG");
    }
    if (cause == RESET_SECURITY) {
      LOG_DBG("RESET_SECURITY");
    }
    if (cause == RESET_LOW_POWER_WAKE) {
      LOG_DBG("RESET_LOW_POWER_WAKE");
    }
    if (cause == RESET_CPU_LOCKUP) {
      LOG_DBG("RESET_CPU_LOCKUP");
    }
    if (cause == RESET_PARITY) {
      LOG_DBG("RESET_PARITY");
    }
    if (cause == RESET_PLL) {
      LOG_DBG("RESET_PLL");
    }
    if (cause == RESET_CLOCK) {
      LOG_DBG("RESET_CLOCK");
    }
    if (cause == RESET_HARDWARE) {
      LOG_DBG("RESET_HARDWARE");
    }
    if (cause == RESET_USER) {
      LOG_DBG("RESET_USER");
    }
    if (cause == RESET_TEMPERATURE) {
      LOG_DBG("RESET_TEMPERATURE");
    }
    hwinfo_clear_reset_cause();
  }
}

void main(void) {
  int err;
  unsigned int min;
  int display_address;
  static Stop stop = {.last_updated = 0, .id = STOP_ID};

  log_reset_reason();

  err = nrf_modem_lib_init(NORMAL_MODE);
  if (err) {
    LOG_ERR("Failed to initialize modem library!");
    goto reset;
  }

  if (set_rtc_time() != 0) {
    LOG_ERR("Failed to set rtc.");
    goto reset;
  }

  while (1) {
    min = 0;

    err = http_request_json();
    if (err != 200) {
      LOG_ERR("HTTP GET request for JSON failed; cleaning up. ERR: %d", err);
      goto reset;
    }

    err = parse_json_for_stop(recv_body_buf, &stop);
    if (err) {
      LOG_DBG(
          "recv_body_buf size: %d, recv_body strlen: %d", sizeof(recv_body_buf),
          strlen(recv_body_buf)
      );
      LOG_DBG("recv_body_buf:\n%s", recv_body_buf);
      goto reset;
    }

    LOG_DBG(
        "Stop ID: %s\nStop routes size: %d\nLast updated: %lld\n", stop.id,
        stop.routes_size, stop.last_updated
    );

    for (int i = 0; i < stop.routes_size; i++) {
      struct RouteDirection route_direction = stop.route_directions[i];
      LOG_INF(
          "\n========= Route ID: %d; Direction: %c; Departures size: %d "
          "========= ",
          route_direction.id, route_direction.direction_code,
          route_direction.departures_size
      );
      for (int j = 0; j < route_direction.departures_size; j++) {
        struct Departure departure = route_direction.departures[j];

        min = minutes_to_departure(&departure);
        LOG_INF("Display text: %s", departure.display_text);
        LOG_INF("Minutes to departure: %d", min);

        display_address = get_display_address(
            route_direction.id, route_direction.direction_code,
            departure.display_text
        );

        if (display_address != -1) {
          LOG_INF("Display address: %d", display_address);
          // There is currently no light sensor to adjust brightness
          err = write_num_to_display(display_address, 0x55, min);
          if (err != 0) {
            goto reset;
          }
        } else {
          LOG_WRN(
              "Display address for Route: %d, Direction Code: %c not found.",
              route_direction.id, route_direction.direction_code
          );
        }
      }
    }
    k_msleep(30000);
  }

reset:
  LOG_WRN("Reached end of main; rebooting.");
  /* In ARM implementation sys_reboot ignores the parameter */
  sys_reboot(SYS_REBOOT_COLD);
}
