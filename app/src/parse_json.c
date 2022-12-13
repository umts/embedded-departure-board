//#include <data/json.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(parse_json, LOG_LEVEL_DBG);
#include <cJSON.h>
#include <cJSON_os.h>
void cJSON_Init(void);

int in_char_array(char *array[], char *item, int arr_len) {
  for (int i = 0; i < arr_len; i++) {
    // LOG_INF("array[i]: %s\n", array[i]);
    // LOG_INF("item: %s\n", item);
    if (array[i] == NULL) {
      return 1;
      // LOG_INF("array item is NULL\n");
    }
    else if (strcmp(array[i], item) == 0) {
      return 1;
      // LOG_INF("item is in array\n");
    }
  }
  return 0;
}

int get_departures(char *json) {
  const cJSON *route_direction = NULL;
  const cJSON *route_directions = NULL;
  const cJSON *departure = NULL;
  const cJSON *departures = NULL;
  const cJSON *sdt = NULL;
  const cJSON *ssrl = NULL;
  const cJSON *trip = NULL;
  const cJSON *isd = NULL;
  int departure_size = 0;
  //routes = {"38": "20038", "R29": "10029", "B43": "30043", "B43E": "30943"}

  cJSON *stops_json = cJSON_Parse(json);

  if (stops_json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      LOG_ERR("Error before: %s\n", error_ptr);
    }
    cJSON_Delete(stops_json);
    return 1;
  }

  route_directions = cJSON_GetObjectItemCaseSensitive(stops_json, "RouteDirections");

  if((cJSON_GetArraySize(route_directions) < 1)) {
    LOG_ERR("RouteDirections size is not greater than 0\n");
    cJSON_Delete(stops_json);
    return 1;
  }

  // Get the number of departures so we know what size array to create for unique_isds
  cJSON_ArrayForEach(route_direction, route_directions) {
    departures = cJSON_GetObjectItemCaseSensitive(route_direction, "Departures");
    departure_size += cJSON_GetArraySize(departures);
  }

  // Create unique_isds array based on number of departures
  char *unique_isds[departure_size];
  // for (int j = 0; j < departure_size; j++) {
  //   unique_isds[j] = NULL;
  // }

  //LOG_INF("Departures size: %d\n", departure_size);
  int isd_counter = 0;

  cJSON_ArrayForEach(route_direction, route_directions) {
    departures = cJSON_GetObjectItemCaseSensitive(route_direction, "Departures");
    cJSON_ArrayForEach(departure, departures) {
      sdt = cJSON_GetObjectItemCaseSensitive(departure, "SDT");
      ssrl = cJSON_GetObjectItemCaseSensitive(departure, "StopStatusReportLabel");

      // Verify SDT is a string
      if (!cJSON_IsString(sdt)) {
        LOG_ERR("SDT is not a string\n");
      }

      /* SDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
      We don't care about the time zone (plus it's wrong), so we just want the seconds. We strip
      off the leading '/Date(' and trailing '-400)\' + the last 3 digits to do a rough conversion to seconds. */
      char *sdt_string = (sdt->valuestring) + 6;
      // Subtracting from the end of the string i.e. strlen(sdt_string) gives doesn't work for some reason
      sdt_string[10] = '\0';
      int sdt_seconds = atoi(sdt_string);
      //LOG_INF("SDT: %d\n", sdt_seconds);

      // Verify the departure isn't skipped
      if (!cJSON_IsString(ssrl)) {
        LOG_ERR("StopStatusReportLabel is not a string\n");
      } else if (strcmp(ssrl->valuestring, "Skipped") == 0) {
        LOG_INF("StopStatusReportLabel == Skipped");
      }

      trip = cJSON_GetObjectItemCaseSensitive(departure, "Trip");
      isd = cJSON_GetObjectItemCaseSensitive(trip, "InternetServiceDesc");
      //LOG_INF("InternetServiceDesc: %s\n", isd->valuestring);

      if (cJSON_IsString(isd)) {
        if (isd_counter == 0) {
          unique_isds[isd_counter] = isd->valuestring;
        }
        if (!in_char_array(unique_isds, (isd->valuestring), isd_counter)) {
          //unique_isds[isd_counter] = isd->valuestring;
          LOG_INF("item not in array\n");
        }
        unique_isds[isd_counter] = isd->valuestring;
        //LOG_INF("InternetServiceDesc not in array: %s\n", isd->valuestring);
        // LOG_INF("unique_isds[i]: %s\n", unique_isds[isd_counter]);
      } else {
        LOG_ERR("InternetServiceDesc is not a string\n");
      }
      isd_counter++;
      // LOG_INF("i end: %d\n", isd_counter);
    }
  }
  
  // for (int i = 0; i < departure_size; i++) {
  //   LOG_INF("InternetServiceDesc: %s\n", unique_isds[i]);
  // }

  cJSON_Delete(stops_json);
  return 0;
}

void parse_json(void) {
  char *json_ptr = "[{\"LastUpdated\":\"/Date(1653408920673-0400)/\",\"RouteDirections\":[{\"Departures\":[],\"Direction\":\"Northbound\",\"DirectionCode\":\"N\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":10029,\"RouteRecordId\":1888},{\"Departures\":[{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1653411600000-0400)/\",\"EDTLocalTime\":\"2022-05-24T13:00:00\",\"ETA\":\"/Date(1653411551000-0400)/\",\"ETALocalTime\":\"2022-05-24T12:59:11\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653408813380-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T12:13:33\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653411600000-0400)/\",\"SDTLocalTime\":\"2022-05-24T13:00:00\",\"STA\":\"/Date(1653411480000-0400)/\",\"STALocalTime\":\"2022-05-24T12:58:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":11,\"GtfsTripId\":\"t514-bB-sl5B\",\"InternalSignDesc\":\"Springfield via Holyoke\",\"InternetServiceDesc\":\"Springfield via Holyoke\",\"IVRServiceDesc\":\"Springfield via Holyoke\",\"StopSequence\":0,\"TripDirection\":\"S\",\"TripId\":1300,\"TripRecordId\":321333,\"TripStartTime\":\"/Date(-2208924000000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T13:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"SATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1653418800000-0400)/\",\"EDTLocalTime\":\"2022-05-24T15:00:00\",\"ETA\":\"/Date(1653418680000-0400)/\",\"ETALocalTime\":\"2022-05-24T14:58:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653408620267-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T12:10:20\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653418800000-0400)/\",\"SDTLocalTime\":\"2022-05-24T15:00:00\",\"STA\":\"/Date(1653418680000-0400)/\",\"STALocalTime\":\"2022-05-24T14:58:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":57,\"GtfsTripId\":\"t5DC-b39-sl5B\",\"InternalSignDesc\":\"Springfield via Holyoke\",\"InternetServiceDesc\":\"Springfield via Holyoke\",\"IVRServiceDesc\":\"Springfield via Holyoke\",\"StopSequence\":0,\"TripDirection\":\"S\",\"TripId\":1500,\"TripRecordId\":321336,\"TripStartTime\":\"/Date(-2208916800000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T15:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"SATCO\"}],\"Direction\":\"Southbound\",\"DirectionCode\":\"S\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":10029,\"RouteRecordId\":1888},{\"Departures\":[{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:03:23\",\"EDT\":\"/Date(1653410303000-0400)/\",\"EDTLocalTime\":\"2022-05-24T12:38:23\",\"ETA\":\"/Date(1653410303000-0400)/\",\"ETALocalTime\":\"2022-05-24T12:38:23\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653408918343-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T12:15:18\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653410100000-0400)/\",\"SDTLocalTime\":\"2022-05-24T12:35:00\",\"STA\":\"/Date(1653410100000-0400)/\",\"STALocalTime\":\"2022-05-24T12:35:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4303,\"GtfsTripId\":\"t4B0-b10CF-sl3\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":2100,\"TripDirection\":\"E\",\"TripId\":1200,\"TripRecordId\":58049,\"TripStartTime\":\"/Date(-2208927600000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T12:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1653413700000-0400)/\",\"EDTLocalTime\":\"2022-05-24T13:35:00\",\"ETA\":\"/Date(1653413700000-0400)/\",\"ETALocalTime\":\"2022-05-24T13:35:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653379211987-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T04:00:11\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653413700000-0400)/\",\"SDTLocalTime\":\"2022-05-24T13:35:00\",\"STA\":\"/Date(1653413700000-0400)/\",\"STALocalTime\":\"2022-05-24T13:35:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4304,\"GtfsTripId\":\"t514-b10D0-sl3\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":2100,\"TripDirection\":\"E\",\"TripId\":1300,\"TripRecordId\":57751,\"TripStartTime\":\"/Date(-2208924000000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T13:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1653417300000-0400)/\",\"EDTLocalTime\":\"2022-05-24T14:35:00\",\"ETA\":\"/Date(1653417300000-0400)/\",\"ETALocalTime\":\"2022-05-24T14:35:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653408918343-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T12:15:18\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653417300000-0400)/\",\"SDTLocalTime\":\"2022-05-24T14:35:00\",\"STA\":\"/Date(1653417300000-0400)/\",\"STALocalTime\":\"2022-05-24T14:35:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4303,\"GtfsTripId\":\"t578-b10CF-sl3\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":2100,\"TripDirection\":\"E\",\"TripId\":1400,\"TripRecordId\":57954,\"TripStartTime\":\"/Date(-2208920400000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T14:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1653420900000-0400)/\",\"EDTLocalTime\":\"2022-05-24T15:35:00\",\"ETA\":\"/Date(1653420900000-0400)/\",\"ETALocalTime\":\"2022-05-24T15:35:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653379211987-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T04:00:11\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653420900000-0400)/\",\"SDTLocalTime\":\"2022-05-24T15:35:00\",\"STA\":\"/Date(1653420900000-0400)/\",\"STALocalTime\":\"2022-05-24T15:35:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4304,\"GtfsTripId\":\"t5DC-b10D0-sl3\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":2100,\"TripDirection\":\"E\",\"TripId\":1500,\"TripRecordId\":57527,\"TripStartTime\":\"/Date(-2208916800000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T15:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"}],\"Direction\":\"East\",\"DirectionCode\":\"E\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":30043,\"RouteRecordId\":272},{\"Departures\":[{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:18\",\"EDT\":\"/Date(1653409818000-0400)/\",\"EDTLocalTime\":\"2022-05-24T12:30:18\",\"ETA\":\"/Date(1653409818000-0400)/\",\"ETALocalTime\":\"2022-05-24T12:30:18\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653408920673-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T12:15:20\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653409800000-0400)/\",\"SDTLocalTime\":\"2022-05-24T12:30:00\",\"STA\":\"/Date(1653409800000-0400)/\",\"STALocalTime\":\"2022-05-24T12:30:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4301,\"GtfsTripId\":\"t4BF-b10CD-sl3\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":1215,\"TripRecordId\":57784,\"TripStartTime\":\"/Date(-2208926700000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T12:15:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:01:23\",\"EDT\":\"/Date(1653411983000-0400)/\",\"EDTLocalTime\":\"2022-05-24T13:06:23\",\"ETA\":\"/Date(1653411983000-0400)/\",\"ETALocalTime\":\"2022-05-24T13:06:23\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653408918343-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T12:15:18\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653411900000-0400)/\",\"SDTLocalTime\":\"2022-05-24T13:05:00\",\"STA\":\"/Date(1653411900000-0400)/\",\"STALocalTime\":\"2022-05-24T13:05:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4303,\"GtfsTripId\":\"t4E2-b10CF-sl3\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":1250,\"TripRecordId\":58071,\"TripStartTime\":\"/Date(-2208924600000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T12:50:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1653415500000-0400)/\",\"EDTLocalTime\":\"2022-05-24T14:05:00\",\"ETA\":\"/Date(1653415500000-0400)/\",\"ETALocalTime\":\"2022-05-24T14:05:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653379211987-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T04:00:11\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653415500000-0400)/\",\"SDTLocalTime\":\"2022-05-24T14:05:00\",\"STA\":\"/Date(1653415500000-0400)/\",\"STALocalTime\":\"2022-05-24T14:05:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4304,\"GtfsTripId\":\"t546-b10D0-sl3\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":1350,\"TripRecordId\":57432,\"TripStartTime\":\"/Date(-2208921000000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T13:50:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1653419100000-0400)/\",\"EDTLocalTime\":\"2022-05-24T15:05:00\",\"ETA\":\"/Date(1653419100000-0400)/\",\"ETALocalTime\":\"2022-05-24T15:05:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1653408918343-0400)/\",\"LastUpdatedLocalTime\":\"2022-05-24T12:15:18\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1653419100000-0400)/\",\"SDTLocalTime\":\"2022-05-24T15:05:00\",\"STA\":\"/Date(1653419100000-0400)/\",\"STALocalTime\":\"2022-05-24T15:05:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4303,\"GtfsTripId\":\"t5AA-b10CF-sl3\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":1450,\"TripRecordId\":57752,\"TripStartTime\":\"/Date(-2208917400000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T14:50:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"}],\"Direction\":\"West\",\"DirectionCode\":\"W\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":30043,\"RouteRecordId\":272}],\"StopId\":73,\"StopRecordId\":14853}";

  // The json response is in an array, since we're only requesting one item we can manually remove
  // the square brackets before parsing the bytestring to stop the parser from thinking it's a list
  json_ptr++;
  // json_ptr[strlen(json_ptr) - 1] = 0;
  // LOG_INF("%c\n", json_ptr[strlen(json_ptr) - 1]);
  // LOG_INF("%s\n", json_ptr);
  get_departures(json_ptr);
}