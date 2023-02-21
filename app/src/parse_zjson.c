#include <parse_zjson.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/data/json.h>

/* Newlib C includes */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

/* app includes */
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(parse_json, LOG_LEVEL_DBG);

static const struct json_obj_descr trip_descr[] = {
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "BlockFareboxId", block_farebox_id, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "GtfsTripId", gtfs_trip_id, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "InternalSignDesc", internal_sign_desc, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "InternetServiceDesc", internet_sign_desc, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "IVRServiceDesc", ivr_sign_dec, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "StopSequence", stop_sequence, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "TripDirection", trip_direction, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "TripId", trip_id, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "TripRecordId", trip_record_id, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "TripStartTime", trip_start_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "TripStartTimeLocalTime", trip_start_time_local_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "TripStatus", trip_status, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_TRIP, "TripStatusReportLabel", trip_status_report_label, JSON_TOK_STRING)
};

static const struct json_obj_descr departures_descr[] = {
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "ADT", adt, JSON_TOK_TRUE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "ADTLocalTime", adt_local_time, JSON_TOK_TRUE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "ATA", ata, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "ATALocalTime", ata_local_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "Bay", bay, JSON_TOK_TRUE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "Dev", dev, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "EDT", edt, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "EDTLocalTime", edt_local_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "ETA", eta, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "ETALocalTime", eta_local_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "IsCompleted", completed, JSON_TOK_FALSE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "IsLastStopOnTrip", last_stop_on_trip, JSON_TOK_FALSE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "LastUpdated", last_updated, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "LastUpdatedLocalTime", last_updated_local_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "Mode", mode, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "ModeReportLabel", mode_report_label, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "PropogationStatus", propogation_status, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "SDT", sdt, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "SDTLocalTime", sdt_local_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "STA", sta, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "STALocalTime", sta_local_time, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "StopFlag", stop_flag, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "StopStatus", stop_status, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "StopStatusReportLabel", stop_status_report_label, JSON_TOK_STRING),
  JSON_OBJ_DESCR_OBJECT_NAMED(JSON_DEPARTURE, "Trip", trip, trip_descr),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_DEPARTURE, "PropertyName", property_name, JSON_TOK_STRING)
};

static const struct json_obj_descr route_directions_descr[] = {
  JSON_OBJ_DESCR_OBJ_ARRAY_NAMED(
    JSON_ROUTE_DIRECTION, "Departures", departures, MAX_DEPARTURES,
    departures_size, departures_descr, ARRAY_SIZE(departures_descr)
  ),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "Direction", direction, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "DirectionCode", direction_code, JSON_TOK_STRING),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "HeadwayDepartures", headway_departures, JSON_TOK_TRUE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "IsDone", done, JSON_TOK_FALSE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "IsHeadway", headway, JSON_TOK_FALSE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "IsHeadwayMonitored", headway_monitored, JSON_TOK_FALSE),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "RouteId", route_id, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_ROUTE_DIRECTION, "RouteRecordId", route_record_id, JSON_TOK_NUMBER)
};

static const struct json_obj_descr stop_descr[] = {
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_STOP, "LastUpdated", last_updated, JSON_TOK_STRING),
  JSON_OBJ_DESCR_OBJ_ARRAY_NAMED(
    JSON_STOP, "RouteDirections", directions, MAX_ROUTES,
    directions_size, route_directions_descr, ARRAY_SIZE(route_directions_descr)
  ),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_STOP, "StopId", id, JSON_TOK_NUMBER),
  JSON_OBJ_DESCR_PRIM_NAMED(JSON_STOP, "StopRecordId", record_id, JSON_TOK_NUMBER)
};

// Route* pluck_departure_data(JSON_ROUTE_DIRECTION *parsed_route, Route *route) {

  // JSON_DEPARTURE parsed_departures = parsed_route.departures
//   cJSON *departure_json = NULL;
//   int valid_dep_count = 0;
//   char *unique_isds[MAX_DEPARTURES] = { NULL };

//   cJSON_ArrayForEach(departure_json, departures_json) {
//     // Verify and assign isd
//     cJSON *trip = cJSON_GetObjectItemCaseSensitive(departure_json, "Trip");
//     cJSON *isd = cJSON_GetObjectItemCaseSensitive(trip, "InternetServiceDesc");
//     if (!cJSON_IsString(isd)) {
//       LOG_ERR("InternetServiceDesc is not a string");
//       return NULL;
//     }

//     // Verify and assign edt
//     cJSON *edt = cJSON_GetObjectItemCaseSensitive(departure_json, "EDT");
//     if (!cJSON_IsString(edt)) {
//       LOG_ERR("EDT is not a string");
//       return NULL;
//     }
  
//     /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
//     *  We don't care about the time zone (plus it's wrong), so we just want the seconds.
//     *  We strip off the leading '/Date(' and trailing '-400)\' + the last 3 digits to do
//     *  a rough conversion to seconds.
//     */
//     char *edt_string = (edt->valuestring) + 6;
//     edt_string[10] = '\0';
//     int edt_seconds = (time_t)atoll(edt_string);

//     if (
//       isd_unique(unique_isds, isd->valuestring, valid_dep_count) &&
//       (edt_seconds > get_rtc_time())
//     ) {

//       // Push unique isd to array
//       unique_isds[valid_dep_count] = isd->valuestring;

//       Departure *new_departure = &route->departures[valid_dep_count];
//       //strcpy(new_departure->isd, isd->valuestring);
//       new_departure->isd = isd->valuestring;
//       new_departure->etd = edt_seconds;

//       // Verify and assign departure skipped status
//       cJSON *ssrl = cJSON_GetObjectItemCaseSensitive(departure_json, "StopStatusReportLabel");
//       if (!cJSON_IsString(ssrl)) {
//         LOG_ERR("StopStatusReportLabel is not a string");
//         return NULL;
//       } else if (strcmp(ssrl->valuestring, "Skipped")) {
//         new_departure->skipped = false;
//       } else {
//         new_departure->skipped = true;
//       }

//       // Verify and assign sdt
//       cJSON *sdt = cJSON_GetObjectItemCaseSensitive(departure_json, "SDT");
//       if (!cJSON_IsString(sdt)) {
//         LOG_ERR("SDT is not a string");
//         return NULL;
//       } else {
//         char *sdt_string = (sdt->valuestring) + 6;
//         sdt_string[10] = '\0';
//         new_departure->std = (time_t)atoll(sdt_string);
//       }

//       cJSON *trip_id = cJSON_GetObjectItemCaseSensitive(trip, "TripId");
//       if (!cJSON_IsNumber(trip_id)) {
//         LOG_ERR("TripId is not a number");
//         return NULL;
//       } else {
//         new_departure->id = trip_id->valueint;
//       }

//       //route->departures[valid_dep_count] = *new_departure;
//       valid_dep_count++;
//     }
//   }
//   route->departures_size = valid_dep_count;
//   return route;
// }

void json_replace_null_values(char *json_string) {
  static const char* old_word = "null";  
  static const char* new_word = "true";
  char *ret;

  while((ret = strstr(json_string, old_word))) {
    memcpy(ret, new_word, strlen(new_word));
  }
}

int parse_json_for_stop(JSON_STOP *stop, char json_string[]) {
  /* The JSON response is in an unnamed array, since we're only requesting one item we know the stops
   * are at the first index. */
  json_string++;
  json_string[strlen(json_string) - 1] = '\0';

  json_replace_null_values(json_string);
  LOG_INF("\n%s", json_string);

  int ret = json_obj_parse(
    json_string,
    strlen(json_string),
    stop_descr,
    ARRAY_SIZE(stop_descr),
    stop
  );

  if (ret < 0) {
    LOG_ERR("JSON Parse Error: %d", ret);
    return 1;
  }

  // TODO: Select routes form ROUTE_IDS in stop.h

  // for (int routes_count = 0; routes_count < parsed_stop.directions_size; routes_count++) {
  //   ret = pluck_route_data(&parsed_stop.directions[routes_count], stop);
  //   if (ret != 0) {
  //     LOG_ERR("parse_routes failed");
  //   }
  // }

  return 0;
}
