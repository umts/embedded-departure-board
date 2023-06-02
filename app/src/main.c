/* Zephyr includes */
#include <string.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/time.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/types.h>

/* Newlib C includes */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* nrf lib includes */
#include <modem/nrf_modem_lib.h>

/* app includes */
#include <custom_http_client.h>
#include <jsmn_parse.h>
#include <rtc.h>
#include <stop.h>
#include <stop_display_index.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define UART2 DEVICE_DT_GET(DT_NODELABEL(uart2))

const struct device *uart_feather_header = UART2;

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data) {
  switch (evt->type) {
    case UART_TX_DONE:
      LOG_DBG("Tx sent %d bytes", evt->data.tx.len);
      break;
    case UART_TX_ABORTED:
      LOG_ERR("Tx aborted");
      break;
    case UART_RX_RDY:
    case UART_RX_STOPPED:
    case UART_RX_BUF_REQUEST:
    case UART_RX_BUF_RELEASED:
    case UART_RX_DISABLED:
      LOG_ERR("UART rx disabled");
      break;
  }
}

static bool uart_feather_header_ready(void) {
  /* Check device readiness */
  if (!device_is_ready(uart_feather_header)) {
    LOG_WRN("UART device isn't ready! Retrying...");
    k_msleep(1000);
    if (!device_is_ready(uart_feather_header)) {
      return false;
    }
  }
  return true;
}

static uint16_t minutes_to_departure(Departure *departure) {
  int edt_ms = departure->etd;
  return (uint16_t)(edt_ms - get_rtc_time()) / 60;
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
  uint16_t min;
  int display_address;
  unsigned char tx_buf[12];
  static Stop stop = {.last_updated = 0, .id = STOP_ID};

  log_reset_reason();

  if (uart_feather_header_ready()) {
    uart_callback_set(uart_feather_header, uart_cb, NULL);
    uart_rx_disable(uart_feather_header);
  } else {
    LOG_ERR("UART device failed");
    goto reset;
  }

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
    memset(tx_buf, 0, sizeof(tx_buf));

    err = http_request_json();
    if (err != 200) {
      LOG_ERR("HTTP GET request for JSON failed; cleaning up. ERR: %d", err);
      goto reset;
    }

    err = parse_json_for_stop(recv_body_buf, &stop);
    if (err) {
      LOG_DBG("recv_body_buf size: %d, recv_body strlen: %d", sizeof(recv_body_buf),
              strlen(recv_body_buf));
      LOG_DBG("recv_body_buf:\n%s", recv_body_buf);
      goto reset;
    }

    LOG_DBG("Stop ID: %s\nStop routes size: %d\nLast updated: %lld\n", stop.id, stop.routes_size,
            stop.last_updated);

    for (int i = 0; i < stop.routes_size; i++) {
      struct RouteDirection route_direction = stop.route_directions[i];
      LOG_INF("\n========= Route ID: %d; Direction: %c; Departures size: %d =========",
              route_direction.id, route_direction.direction_code, route_direction.departures_size);
      for (int j = 0; j < route_direction.departures_size; j++) {
        struct Departure departure = route_direction.departures[j];

        min = minutes_to_departure(&departure);
        LOG_INF("Display text: %s", departure.display_text);
        LOG_INF("Minutes to departure: %d", min);

        display_address = get_display_address(route_direction.id, route_direction.direction_code,
                                              departure.display_text);

        if (display_address != -1) {
          LOG_INF("Display address: %d", display_address);
          tx_buf[display_address] = ((min >> 8) & 0xFF);
          tx_buf[display_address + 1] = (min & 0xFF);
        } else {
          LOG_WRN("I2C display address for Route: %d, Direction Code: %c not found.",
                  route_direction.id, route_direction.direction_code);
        }
      }
    }

    int err = uart_tx(uart_feather_header, tx_buf, sizeof(tx_buf), 10000);
    if (err != 0) {
      LOG_ERR("Tx err %d", err);
    }
    k_msleep(30000);
  }

reset:
  LOG_WRN("Reached end of main; rebooting.");
  /* In ARM implementation sys_reboot ignores the parameter */
  sys_reboot(SYS_REBOOT_COLD);
}
