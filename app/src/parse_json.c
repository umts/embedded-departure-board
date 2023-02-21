#include <parse_json.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>

/* Newlib C includes */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

/* nrf lib includes */
#include <cJSON.h>
#include <cJSON_os.h>

/* app includes */
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(parse_json, LOG_LEVEL_DBG);
void cJSON_Init(void);

bool isd_unique(char *array[], char *isd, int arr_len) {
  if (isd[strlen(isd)] != '\0') {
    LOG_ERR("ISD is not zero terminated");
  }

  /* Array is empty, return true */
  // if (arr_len == 0) {
  //   return true;
  // }

  for (int i = 0; i < arr_len; i++) {
    LOG_WRN("isd: %s", isd);
    if (strcmp(array[i], isd) == 0) {
      return false;
    }
  }
  return true;
}

Route* parse_departures(cJSON *departures_json, Route *route) {
  cJSON *departure_json = NULL;
  int valid_dep_count = 0;
  char *unique_isds[MAX_DEPARTURES] = { NULL };

  cJSON_ArrayForEach(departure_json, departures_json) {
    // Verify and assign isd
    cJSON *trip = cJSON_GetObjectItemCaseSensitive(departure_json, "Trip");
    cJSON *isd = cJSON_GetObjectItemCaseSensitive(trip, "InternetServiceDesc");
    if (!cJSON_IsString(isd)) {
      LOG_ERR("InternetServiceDesc is not a string");
      return NULL;
    }

    // Verify and assign edt
    cJSON *edt = cJSON_GetObjectItemCaseSensitive(departure_json, "EDT");
    if (!cJSON_IsString(edt)) {
      LOG_ERR("EDT is not a string");
      return NULL;
    }
  
    /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
    *  We don't care about the time zone (plus it's wrong), so we just want the seconds.
    *  We strip off the leading '/Date(' and trailing '-400)\' + the last 3 digits to do
    *  a rough conversion to seconds.
    */
    char *edt_string = (edt->valuestring) + 6;
    edt_string[10] = '\0';
    int edt_seconds = (time_t)atoll(edt_string);

    if (
      isd_unique(unique_isds, isd->valuestring, valid_dep_count) &&
      (edt_seconds > get_rtc_time())
    ) {

      // Push unique isd to array
      unique_isds[valid_dep_count] = isd->valuestring;

      Departure *new_departure = &route->departures[valid_dep_count];
      //strcpy(new_departure->isd, isd->valuestring);
      new_departure->isd = isd->valuestring;
      new_departure->etd = edt_seconds;

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

      //route->departures[valid_dep_count] = *new_departure;
      valid_dep_count++;
    }
  }
  route->departures_size = valid_dep_count;
  return route;
}

Stop* parse_routes(cJSON *route_directions, Stop *stop) {
  cJSON *route_direction = NULL;
  int routes_count = 0;

  // TODO: Select routes form ROUTE_IDS in stop.h

  cJSON_ArrayForEach(route_direction, route_directions) {
    Route *new_route = &stop->routes[routes_count];

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

    //stop->routes[routes_count] = new_route;
    routes_count++;
  }

  stop->routes_size = routes_count;
  return stop;
}

void parse_json_for_stop(Stop *stop, char *json_string) {
  // cJSON *stops_json = cJSON_ParseWithLength(json_string, sizeof(json_string) + 1);
  cJSON *stops_json = cJSON_Parse(json_string);

  if (stops_json == NULL) {
    LOG_ERR("JSON pointer NULL!");
    // LOG_INF("%s\n", json_string);
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      LOG_INF("%s\n", error_ptr);   
    }
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
}