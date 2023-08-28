#ifndef STOP_H
#define STOP_H

#define RETRY_COUNT 3

/* Edit this to chose what stop you want to query */
#define STOP_ID "73"

/** @def MAX_ROUTES
 *  @brief A macro that defines the maximum number of routes expected for a stop.
 *
 * Specify the maximum number of routes you expect your selected stop to have.
 */
#define MAX_ROUTES 6

/** @def MAX_DEPARTURES
 *  @brief A macro that defines the maximum number of departures expected for a route.
 *
 *  Specify the maximum number of departures you expect each route to have.
 *  Applies to **ALL**  departures.
 */
#define MAX_DEPARTURES 4

typedef struct Departure {
  char display_text[50];
  unsigned int etd;
} Departure;

typedef struct RouteDirection {
  char direction_code;
  int id;
  unsigned int departures_size;
  Departure departures[MAX_DEPARTURES];
} RouteDirection;

typedef struct Stop {
  unsigned long long last_updated;
  const char *id;
  unsigned int routes_size;
  RouteDirection route_directions[MAX_ROUTES];
} Stop;
#endif
