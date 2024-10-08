#include "update_stop.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "custom_http_client.h"
#include "display_switches.h"
#include "external_rtc.h"
#include "json/jsmn_parse.h"
#include "led_display.h"
#include "stop.h"

LOG_MODULE_REGISTER(update_stop, LOG_LEVEL_INF);

K_TIMER_DEFINE(update_stop_timer, update_stop_timeout_handler, NULL);

K_SEM_DEFINE(stop_sem, 1, 1);

static unsigned int minutes_to_departure(Departure* departure) {
  int edt_ms = departure->etd;
  return (unsigned int)(edt_ms - get_external_rtc_time()) / 60;
}

static DisplayBox* get_display_address(
    const DisplayBox display_boxes[], const int route_id,
    const char direction_code
) {
  for (size_t box = 0; box < NUMBER_OF_DISPLAY_BOXES; box++) {
    if ((route_id == display_boxes[box].id) &&
        (display_boxes[box].direction_code == direction_code)) {
      return &display_boxes[box];
    }
  }
  return NULL;
}

static int parse_returned_routes(Stop stop, DisplayBox display_boxes[]) {
  unsigned int min = 0;

  unsigned int times[6] = {0, 0, 0, 0, 0, 0};

  for (size_t box = 0; box < NUMBER_OF_DISPLAY_BOXES; box++) {
    (void)display_off(box);
  }

  for (size_t route_num = 0; route_num < stop.routes_size; route_num++) {
    struct RouteDirection route_direction = stop.route_directions[route_num];
    LOG_INF(
        "\n========= Route ID: %d; Direction: %c; Departures size: %d "
        "========= ",
        route_direction.id, route_direction.direction_code,
        route_direction.departures_size
    );
    for (size_t departure_num = 0;
         departure_num < route_direction.departures_size; departure_num++) {
      struct Departure departure = route_direction.departures[departure_num];
      min = minutes_to_departure(&departure);
      LOG_INF("Display text: %s", departure.display_text);
      LOG_INF("Minutes to departure: %d", min);

      DisplayBox* display = get_display_address(
          display_boxes, route_direction.id, route_direction.direction_code
      );
      if (display != NULL) {
        LOG_INF("Display address: %d", display->position);
        if ((times[display->position] == 0) ||
            (min < times[display->position])) {
          times[display->position] = min;
          if (write_num_to_display(display, display->brightness, min)) {
            return 1;
          }
        }
#ifdef CONFIG_DEBUG
        else {
          LOG_WRN(
              "Display %u has lower time displayed;\nCurrent: %u\nAttempted: "
              "%u",
              display->position, times[display->position], min
          );
        }
#endif

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
  static Stop stop = {.last_updated = 0, .id = CONFIG_STOP_ID};
  static const DisplayBox display_boxes[] = DISPLAY_BOXES;

  static char headers_buf[1024];

  /** HTTP response body buffer with size defined by the
   * CONFIG_STOP_JSON_BUF_SIZE
   */
  static char json_buf[CONFIG_STOP_JSON_BUF_SIZE];

  err = http_request_stop_json(
      &json_buf[0], CONFIG_STOP_JSON_BUF_SIZE, headers_buf, sizeof(headers_buf)
  );
  if (err) {
    LOG_ERR("HTTP GET request for JSON failed; cleaning up. ERR: %d", err);
    return 1;
  }

  err = parse_stop_json(&json_buf[0], &stop);
  if (err) {
    LOG_DBG(
        "recv_body_buf size: %d, recv_body strlen: %d",
        CONFIG_STOP_JSON_BUF_SIZE, strlen(&json_buf[0])
    );
    LOG_DBG("recv_body_buf:\n%s", &json_buf[0]);
    return 1;
  }

  LOG_DBG(
      "Stop ID: %s\nStop routes size: %d\nLast updated: %llu\n", stop.id,
      stop.routes_size, stop.last_updated
  );

  err = parse_returned_routes(stop, display_boxes);
  if (err) {
    return 1;
  }

  return 0;
}

void update_stop_timeout_handler(struct k_timer* timer_id) {
  LOG_DBG("Update stop timeout\n");
  (void)k_sem_give(&stop_sem);
}
