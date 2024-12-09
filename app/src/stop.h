#ifndef STOP_H
#define STOP_H

typedef struct Departure {
  char display_text[50];
  unsigned int etd;
} Departure;

typedef struct RouteDirection {
  char direction_code;
  int id;
  unsigned int departures_size;
  Departure departures[CONFIG_ROUTE_MAX_DEPARTURES];
} RouteDirection;

typedef struct Stop {
  unsigned long long last_updated;
  const char *id;
  unsigned int routes_size;
  RouteDirection route_directions[CONFIG_STOP_MAX_ROUTES];
} Stop;
#endif
