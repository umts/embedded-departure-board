#include "update_stop.h"

/* app includes */
#include <app_rtc.h>
#include <custom_http_client.h>
#include <jsmn_parse.h>
// #include <led_display.h>
#include <stop.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(update_stop, LOG_LEVEL_DBG);

K_TIMER_DEFINE(update_stop_timer, update_stop_timeout_handler, NULL);

K_SEM_DEFINE(stop_sem, 1, 1);

static unsigned int minutes_to_departure(Departure* departure) {
  int edt_ms = departure->etd;
  return (unsigned int)(edt_ms - get_app_rtc_time()) / 60;
}

static int get_display_address(
    const DisplayBox display_boxes[], const int route_id,
    const char direction_code
) {
  for (size_t box = 0; box < NUMBER_OF_DISPLAY_BOXES; box++) {
    if ((route_id == display_boxes[box].id) &&
        (display_boxes[box].direction_code == direction_code)) {
      return display_boxes[box].position;
    }
  }
  return -1;
}

static int parse_returned_routes(Stop stop, DisplayBox display_boxes[]) {
  int display_address;
  unsigned int min = 0;

  // for (size_t box = 0; box < NUMBER_OF_DISPLAY_BOXES; box++) {
  //   (void)turn_display_off(box);
  // }

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
          display_boxes, route_direction.id, route_direction.direction_code
      );

      if (display_address != -1) {
        LOG_INF("Display address: %d", display_address);
        // There is currently no light sensor to adjust brightness
        // if (write_num_to_display(display_address, 0x33, min)) {
        //   return 1;
        // }
      } else {
        LOG_WRN(
            "Display address for Route: %d, Direction Code: %c not found.",
            route_direction.id, route_direction.direction_code
        );
      }
    }
  }
  return 0;
}

int update_stop(void) {
  int err;
  static Stop stop = {.last_updated = 0, .id = STOP_ID};
  static const DisplayBox display_boxes[] = DISPLAY_BOXES;

  err = http_request_stop_json();
  if (err != 200) {
    LOG_ERR("HTTP GET request for JSON failed; cleaning up. ERR: %d", err);
    return 1;
  }

  err = parse_json_for_stop(recv_body_buf, &stop);
  if (err) {
    LOG_DBG(
        "recv_body_buf size: %d, recv_body strlen: %d", sizeof(recv_body_buf),
        strlen(recv_body_buf)
    );
    LOG_DBG("recv_body_buf:\n%s", recv_body_buf);
    return 1;
  }

  LOG_DBG(
      "Stop ID: %s\nStop routes size: %d\nLast updated: %lld\n", stop.id,
      stop.routes_size, stop.last_updated
  );

  if (parse_returned_routes(stop, display_boxes)) {
    return 1;
  }

  return 0;
}

void update_stop_timeout_handler(struct k_timer* timer_id) {
  LOG_DBG("Update stop timeout\n");
  (void)k_sem_give(&stop_sem);
}
