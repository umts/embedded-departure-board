#include "update_stop.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "display/display_switches.h"
#include "display/led_display.h"
#include "net/custom_http_client.h"
#include "real_time_counter.h"
#include "stop.h"

#ifdef CONFIG_STOP_REQUEST_SWIFTLY
#include "json/parse_swiftly.h"
#endif  // CONFIG_STOP_REQUEST_SWIFTLY

#ifdef CONFIG_STOP_REQUEST_BUSTRACKER
#include "json/parse_bustracker.h"
#endif  // CONFIG_STOP_REQUEST_BUSTRACKER

LOG_MODULE_REGISTER(update_stop);

K_TIMER_DEFINE(update_stop_timer, update_stop_timeout_handler, NULL);

K_SEM_DEFINE(update_stop_sem, 1, 1);

static DisplayBox* get_display_address(
    const DisplayBox display_boxes[], const char* route_id, const char direction_code
) {
  for (size_t box = 0; box < CONFIG_NUMBER_OF_DISPLAY_BOXES; box++) {
    if (!strncmp(route_id, display_boxes[box].id, 4) &&
        (display_boxes[box].direction_code == direction_code)) {
      return &display_boxes[box];
    }
  }
  return NULL;
}

#ifdef CONFIG_STOP_REQUEST_BUSTRACKER
static unsigned int minutes_to_departure(Departure* departure, unsigned int time_now) {
  int edt_ms = departure->etd;
  return (unsigned int)(edt_ms - time_now) / 60;
}

static int update_routes_bustracker(Stop stop, DisplayBox display_boxes[], unsigned int time_now) {
  unsigned int min = 0;

  unsigned int times[6] = {0, 0, 0, 0, 0, 0};

  for (size_t box = 0; box < CONFIG_NUMBER_OF_DISPLAY_BOXES; box++) {
    (void)display_off(box);
  }

  for (size_t route_num = 0; route_num < stop.routes_size; route_num++) {
    struct RouteDirection route_direction = stop.route_directions[route_num];
    LOG_INF(
        "Route ID: %s;  Direction Code: %c, Destinations size: %d", route_direction.route_id,
        route_direction.direction_code, route_direction.departures_size
    );
    for (size_t departure_num = 0;
         departure_num < route_direction.departures_size; departure_num++) {
      struct Departure departure = route_direction.departures[departure_num];
      min = minutes_to_departure(&departure, time_now);

      DisplayBox* display = get_display_address(
          display_boxes, route_direction.route_id, route_direction.direction_code
      );
      if (display != NULL) {
        LOG_INF("  Display address: %d, Minutes to departure: %d", display->position, min);
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
        LOG_INF(
            "Display address for Route: %s, Direction Code: %c not found. Minutes to "
            "departure: %d",
            route_direction.route_id, route_direction.direction_code, min
        );
      }
    }
  }
  return 0;
}
#endif  // CONFIG_STOP_REQUEST_BUSTRACKER

static int update_routes_swiftly(Stop stop, DisplayBox display_boxes[]) {
  unsigned int times[6] = {0, 0, 0, 0, 0, 0};

  for (size_t box = 0; box < CONFIG_NUMBER_OF_DISPLAY_BOXES; box++) {
    (void)display_off(box);
  }

  for (size_t route_num = 0; route_num < stop.routes_size; route_num++) {
    struct PredictionsData prediction_data = stop.predictions_data[route_num];
    LOG_INF(
        "Route ID: %s; Destinations size: %d", prediction_data.route_id,
        prediction_data.destinations_size
    );
    for (size_t departure_num = 0; departure_num < prediction_data.destinations_size;
         departure_num++) {
      struct Destination destination = prediction_data.destinations[departure_num];

      DisplayBox* display =
          get_display_address(display_boxes, prediction_data.route_id, destination.direction_id);
      if (display != NULL) {
        LOG_INF(
            "  Display address: %d, Direction Code: %c, Minutes to departure: %d",
            display->position, destination.direction_id, destination.min
        );
        if ((times[display->position] == 0) || (destination.min < times[display->position])) {
          times[display->position] = destination.min;
          if (write_num_to_display(display, display->brightness, destination.min)) {
            return 1;
          }
        }
#ifdef CONFIG_DEBUG
        else {
          LOG_WRN(
              "Display %u has lower time displayed;\nCurrent: %u\nAttempted: "
              "%u",
              display->position, times[display->position], destination.min
          );
        }
#endif
      } else {
        LOG_INF(
            "Display address for Route: %s, Direction Code: %c not found. Minutes to "
            "departure: %d",
            prediction_data.route_id, destination.direction_id, destination.min
        );
      }
    }
  }
  return 0;
}

int update_stop(void) {
  int ret;
  unsigned int time_now;
  static Stop stop = {.id = CONFIG_STOP_ID};
  static const DisplayBox display_boxes[] = DISPLAY_BOXES;

  static char headers_buf[2048];
  int json_src = SWIFTLY;

  /** HTTP response body buffer with size defined by the
   * CONFIG_STOP_JSON_BUF_SIZE
   */
  static char json_buf[CONFIG_STOP_JSON_BUF_SIZE];

#ifdef CONFIG_STOP_REQUEST_BUSTRACKER
  // Keep track of retry attempts so we don't get in a loop
  int retry_error = 0;

retry:
#endif  // CONFIG_STOP_REQUEST_BUSTRACKER

  ret = http_request_stop_json(
      &json_buf[0], CONFIG_STOP_JSON_BUF_SIZE, headers_buf, sizeof(headers_buf), &json_src
  );
  if (ret) {
    LOG_ERR("HTTP GET request for JSON failed; cleaning up. ERR: %d", ret);
    return 1;
  }

  time_now = get_rtc_time();

#ifdef CONFIG_STOP_REQUEST_BUSTRACKER
  if (json_src == BUSTRACKER) {
    goto fallback;
  }
#endif  // CONFIG_STOP_REQUEST_BUSTRACKER

  ret = parse_swiftly_json(&json_buf[0], &stop, time_now);
  if (ret) {
    return 1;
  }

  ret = update_routes_swiftly(stop, display_boxes);
  if (ret) {
    return 1;
  }

#ifdef CONFIG_STOP_REQUEST_BUSTRACKER
fallback:
  ret = parse_bustracker_json(&json_buf[0], &stop, time_now);
  if (ret) {
    /* A returned 3 corresponds to an incomplete JSON packet. Most likely this
     * means the HTTP transfer was incomplete. This may be a server-side issue,
     * so we can retry the download once before resetting the device.
     */
    if (ret == 3 && retry_error < CONFIG_HTTP_REQUEST_RETRY_COUNT) {
      LOG_WRN("JSON download was incomplete, retrying...");
      retry_error = 1;
      goto retry;
    } else if (ret == 5) {
      /* A returned 5 corresponds to a successful response with no scheduled
       * departures.
       */
      return 2;
    }

    return 1;
  }
  ret = update_routes_bustracker(stop, display_boxes, time_now);
  if (ret) {
    return 1;
  }
#endif  // CONFIG_STOP_REQUEST_BUSTRACKER

  return 0;
}

void update_stop_timeout_handler(struct k_timer* timer_id) {
  (void)k_sem_give(&update_stop_sem);
}
