#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <cJSON.h>
#include <cJSON_os.h>
#include <routes.h>

LOG_MODULE_REGISTER(parse_json, LOG_LEVEL_DBG);
void cJSON_Init(void);

extern struct Routes *routes[];

struct Routes * get_route_by_id(int id) {
  for (int i = 0; i < (sizeof(&routes)); i++) {
    if (id == routes[i]->id) {
      return routes[i];
    }
  }
  return NULL;
}

// int in_char_array(char *array[], char *item, int arr_len) {
//   for (int i = 0; i < arr_len; i++) {
//     // LOG_INF("array[i]: %s\n", array[i]);
//     // LOG_INF("item: %s\n", item);
//     if (array[i] == NULL) {
//       return 1;
//       // LOG_INF("array item is NULL\n");
//     }
//     else if (strcmp(array[i], item) == 0) {
//       return 1;
//       // LOG_INF("item is in array\n");
//     }
//   }
//   return 0;
// }

void get_departures(char *json_string) {
  cJSON *route_direction = NULL;
  cJSON *route_directions = NULL;
  cJSON *departure = NULL;
  cJSON *departures = NULL;
  cJSON *edt = NULL;
  cJSON *ssrl = NULL;
  // const cJSON *trip = NULL;
  // const cJSON *isd = NULL;
  cJSON *route_id = NULL;
  cJSON *direction_code = NULL;

  cJSON *stops_json = cJSON_Parse(json_string);
  cJSON *stops_child;

  if (stops_json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      LOG_ERR("Error before: %s\n", error_ptr);
    }
    goto end;
  }

  /* The JSON response is in an array, since we're only requesting one item we know the stops
   * are at the first index. */
  if (cJSON_IsArray(stops_json)) {
    stops_child = cJSON_GetArrayItem(stops_json, 0);
    if (stops_child == NULL) {
      LOG_ERR("cJSON_GetArrayItem(stops_json, 0) returned NULL pointer!");
      goto end;
    }
  } else {
    stops_child = stops_json;
  }

  route_directions = cJSON_GetObjectItemCaseSensitive(stops_child, "RouteDirections");

  if((cJSON_GetArraySize(route_directions) < 1)) {
    LOG_ERR("RouteDirections size is not greater than 0\n");
    goto end;
  }

  // Get the number of departures so we know what size array to create for unique_isds
  // cJSON_ArrayForEach(route_direction, route_directions) {
  //   departures = cJSON_GetObjectItemCaseSensitive(route_direction, "Departures");
  //   departure_size += cJSON_GetArraySize(departures);
  // }

  // Create unique_isds array based on number of departures
  // char *unique_isds[departure_size];
  // for (int j = 0; j < departure_size; j++) {
  //   unique_isds[j] = NULL;
  // }

  //LOG_INF("Departures size: %d\n", departure_size);
  // int isd_counter = 0;

  cJSON_ArrayForEach(route_direction, route_directions) {
    departures = cJSON_GetObjectItemCaseSensitive(route_direction, "Departures");
    route_id = cJSON_GetObjectItemCaseSensitive(route_direction, "RouteId");
    direction_code = cJSON_GetObjectItemCaseSensitive(route_direction, "DirectionCode");

    // Verify EDT is a string
    if (!cJSON_IsNumber(route_id)) {
      LOG_ERR("RouteID is not a Number!");
      goto end;
    }

    if (!cJSON_IsString(direction_code)) {
      LOG_ERR("DirectionCode is not a string!");
      goto end;
    }

    struct Routes *route = get_route_by_id(route_id->valueint);

    if (route != NULL) {
      cJSON_ArrayForEach(departure, departures) {
        edt = cJSON_GetObjectItemCaseSensitive(departure, "EDT");
        ssrl = cJSON_GetObjectItemCaseSensitive(departure, "StopStatusReportLabel");

        // Verify EDT is a string
        if (!cJSON_IsString(edt)) {
          LOG_ERR("EDT is not a string");
          goto end;
        }

        // Verify the departure isn't skipped
        if (!cJSON_IsString(ssrl)) {
          LOG_ERR("StopStatusReportLabel is not a string");
          goto end;
        } else if (strcmp(ssrl->valuestring, "Skipped") == 0) {
          LOG_WRN("StopStatusReportLabel: Skipped");
        }

        /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
        We don't care about the time zone (plus it's wrong), so we just want the seconds. We strip
        off the leading '/Date(' and trailing '-400)\' + the last 3 digits to do a rough conversion to seconds. */
        char *edt_string = (edt->valuestring) + 6;
        // Subtracting from the end of the string i.e. strlen(edt_string) gives doesn't work for some reason
        edt_string[10] = '\0';
        int edt_seconds = atoi(edt_string);

        char direction = (direction_code->valuestring[0]);

        if (((direction == 'N') || (direction == 'E')) && (edt_seconds > route->etd[0])) {
          route->etd[0] = edt_seconds;
        } else if ((edt_seconds > route->etd[1])) {
          route->etd[1] = edt_seconds;
        }

        // trip = cJSON_GetObjectItemCaseSensitive(departure, "Trip");
        // isd = cJSON_GetObjectItemCaseSensitive(trip, "InternetServiceDesc");
        // LOG_INF("    InternetServiceDesc: %s\n", isd->valuestring);

        // if (cJSON_IsString(isd)) {
        //   if (isd_counter == 0) {
        //     unique_isds[isd_counter] = isd->valuestring;
        //   }
        //   if (!in_char_array(unique_isds, (isd->valuestring), isd_counter)) {
        //     unique_isds[isd_counter] = isd->valuestring;
        //     // LOG_INF("Adding to array: %s\n", isd->valuestring);
        //   }
        //   // unique_isds[isd_counter] = isd->valuestring;
        //   // LOG_INF("InternetServiceDesc not in array: %s\n", isd->valuestring);
        //   // LOG_INF("unique_isds[i]: %s\n", unique_isds[isd_counter]);
        // } else {
        //   LOG_ERR("InternetServiceDesc is not a string\n");
        // }
        // isd_counter++;
      }
    }
  }

end:
  cJSON_Delete(stops_json);
}