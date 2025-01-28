#ifndef STOP_H
#define STOP_H

#ifdef CONFIG_STOP_REQUEST_BUSTRACKER

typedef struct Departure {
  char display_text[50];
  unsigned int etd;
} Departure;

typedef struct RouteDirection {
  char direction_code;
  char route_id[5];
  unsigned int departures_size;
  Departure departures[CONFIG_ROUTE_MAX_DEPARTURES];
} RouteDirection;

#endif  // CONFIG_STOP_REQUEST_BUSTRACKER

#ifdef CONFIG_STOP_REQUEST_SWIFTLY

typedef struct Destination {
  char direction_id;
  unsigned int min;
} Destination;

typedef struct PredictionsData {
  char route_id[5];
  unsigned int destinations_size;
  Destination destinations[CONFIG_ROUTE_MAX_DEPARTURES];
} PredictionsData;

#endif  // CONFIG_STOP_REQUEST_SWIFTLY

typedef struct Stop {
  const char *id;
  unsigned int routes_size;
#ifdef CONFIG_STOP_REQUEST_BUSTRACKER
  RouteDirection route_directions[CONFIG_STOP_MAX_ROUTES];
#endif  // CONFIG_STOP_REQUEST_BUSTRACKER
#ifdef CONFIG_STOP_REQUEST_SWIFTLY
  PredictionsData predictions_data[CONFIG_STOP_MAX_ROUTES];
#endif  // CONFIG_STOP_REQUEST_SWIFTLY
} Stop;
#endif
