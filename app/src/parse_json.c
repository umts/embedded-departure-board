/* Newlib C includes */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>

/* nrf lib includes */
#include <cJSON.h>
#include <cJSON_os.h>

/* app includes */
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(parse_json, LOG_LEVEL_DBG);
void cJSON_Init(void);

bool isd_unique(char *array[], char *isd, int arr_len) {
  // Array is empty, return true
  if (arr_len == 0) {
    return true;
  }
  for (int i = 0; i < arr_len; i++) {
    if (strcmp(array[i], isd) == 0) {
      return false;
    }
  }
  return true;
}

struct Route* parse_departures(cJSON *departures_json, struct Route *route) {
  cJSON *departure_json = NULL;
  int valid_dep_count = 0;

  cJSON_ArrayForEach(departure_json, departures_json) {
    char **unique_isds = malloc(sizeof(char*));

    // Verify isd and check uniqueness
    cJSON *trip = cJSON_GetObjectItemCaseSensitive(departure_json, "Trip");
    cJSON *isd = cJSON_GetObjectItemCaseSensitive(trip, "InternetServiceDesc");
    if (!cJSON_IsString(isd)) {
      LOG_ERR("InternetServiceDesc is not a string");
      return NULL;
    } else if (isd_unique(unique_isds, isd->valuestring, valid_dep_count)) {

      // Push unique isd to array
      unique_isds = realloc(unique_isds, sizeof(char*) * (valid_dep_count + 1));
      unique_isds[valid_dep_count] = isd->valuestring;

      struct Departure *new_departure = malloc(sizeof(struct Departure));
      new_departure->isd = isd->valuestring;

      // Verify and assign departure skipped status
      cJSON *ssrl = cJSON_GetObjectItemCaseSensitive(departure_json, "StopStatusReportLabel");
      if (!cJSON_IsString(ssrl)) {
        LOG_ERR("StopStatusReportLabel is not a string");
        return NULL;
      } else if (strcmp(ssrl->valuestring, "Skipped")) {
        new_departure->skipped = false;
      } else {
        new_departure->skipped = true;
      }

      // Verify and assign edt
      cJSON *edt = cJSON_GetObjectItemCaseSensitive(departure_json, "EDT");
      if (!cJSON_IsString(edt)) {
        LOG_ERR("EDT is not a string");
        return NULL;
      } else {
        /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
        We don't care about the time zone (plus it's wrong), so we just want the seconds. We strip
        off the leading '/Date(' and trailing '-400)\' + the last 3 digits to do a rough conversion to seconds. */
        char *edt_string = (edt->valuestring) + 6;
        edt_string[10] = '\0';
        new_departure->etd = (time_t)atoll(edt_string);
      }

      // Verify and assign sdt
      cJSON *sdt = cJSON_GetObjectItemCaseSensitive(departure_json, "SDT");
      if (!cJSON_IsString(sdt)) {
        LOG_ERR("SDT is not a string");
        return NULL;
      } else {
        char *sdt_string = (sdt->valuestring) + 6;
        sdt_string[10] = '\0';
        new_departure->std = (time_t)atoll(sdt_string);
      }

      cJSON *trip_id = cJSON_GetObjectItemCaseSensitive(trip, "TripId");
      if (!cJSON_IsNumber(trip_id)) {
        LOG_ERR("TripId is not a number");
        return NULL;
      } else {
        new_departure->id = trip_id->valueint;
      }

      route = realloc(route, sizeof(*route) + (sizeof(*new_departure) * (valid_dep_count + 1)));
      route->departures[valid_dep_count] = *new_departure;
      valid_dep_count++;
    }
    free(unique_isds);
  }
  route->departures_size = valid_dep_count;
  return route;
}

struct Stop* parse_routes(cJSON *route_directions, struct Stop *stop) {
  cJSON *route_direction = NULL;
  int routes_count = 0;

  cJSON_ArrayForEach(route_direction, route_directions) {
    struct Route *new_route = malloc(sizeof(struct Route));

    // Verify and assign route_id
    cJSON *route_id = cJSON_GetObjectItemCaseSensitive(route_direction, "RouteId");
    if (!cJSON_IsNumber(route_id)) {
      LOG_ERR("RouteID is not a Number");
      return NULL;
    } else {
      new_route->id = route_id->valueint;
    }

    // Verify and assign direction_code
    cJSON *direction_code = cJSON_GetObjectItemCaseSensitive(route_direction, "DirectionCode");
    if (!cJSON_IsString(direction_code)) {
      LOG_ERR("DirectionCode is not a string");
      return NULL;
    } else {
      new_route->direction = direction_code->valuestring[0];
    }

    cJSON *departures_json = cJSON_GetObjectItemCaseSensitive(route_direction, "Departures");
    new_route = parse_departures(departures_json, new_route);
    if (new_route == NULL){
      LOG_ERR("parse_departures returned NULL route pointer");
      return NULL;
    }

    stop = realloc(stop, sizeof(*stop) * (sizeof(new_route) * (routes_count + 1)));
    stop->routes[routes_count] = new_route;
    routes_count++;
  }

  stop->id = STOP_ID;
  stop->routes_size = routes_count;
  return stop;
}

struct Stop* parse_stop_json(char *json_string) {
  cJSON *stops_json = cJSON_Parse(json_string);
  struct Stop *stop = malloc(sizeof(struct Stop));

  if (stops_json == NULL) {
    LOG_ERR("JSON pointer NULL!");
    goto end;
  }

  /* The JSON response is in an array, since we're only requesting one item we know the stops
   * are at the first index. */
  if (cJSON_IsArray(stops_json)) {
    stops_json = cJSON_GetArrayItem(stops_json, 0);
    if (stops_json == NULL) {
      LOG_ERR("cJSON_GetArrayItem(stops_json, 0) returned NULL pointer");
      goto end;
    }
  }
  cJSON *route_directions = cJSON_GetObjectItemCaseSensitive(stops_json, "RouteDirections");

  if((cJSON_GetArraySize(route_directions) < 1)) {
    LOG_ERR("RouteDirections size is not greater than 0");
    goto end;
  }
  stop = parse_routes(route_directions, stop);
  if (stop == NULL){
    LOG_ERR("parse_routes returned NULL stop pointer");
  }

end:
  cJSON_Delete(stops_json);
  return stop;
}