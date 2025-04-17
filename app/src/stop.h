#ifndef STOP_H
#define STOP_H

typedef struct Destination {
  char direction_id;
  unsigned int min;
} Destination;

typedef struct PredictionsData {
  char route_id[5];
  unsigned int destinations_size;
  Destination destinations[CONFIG_ROUTE_MAX_DEPARTURES];
} PredictionsData;

typedef struct Stop {
  const char *id;
  unsigned int routes_size;
  PredictionsData predictions_data[CONFIG_STOP_MAX_ROUTES];
} Stop;
#endif
