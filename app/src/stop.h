#ifndef STOPS_H
#define STOPS_H

  /* Edit this to chose what stop you want to query */
  #define STOP_ID "73"

  /* Specify what routes you want to get departure times for, leave blank for all routes */
  #define ROUTE_IDS []

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

  /** @def B43_DISPLAY_ADDR
   *  @brief A macro that defines the I2C display address for route B43.
  */
  #define B43_DISPLAY_ADDR 0x43

  // #include <stdbool.h>

  // struct Departure {
  //   int id;
  //   bool skipped;
  //   char *isd;
  //   int etd;
  //   int std;
  // };

  // struct Route {
  //   char direction;
  //   int id;
  //   int departures_size;
  //   struct Departure departures[];
  // };
  
  // struct Stop {
  //   const char *id;
  //   int routes_size;
  //   struct Route routes[MAX_ROUTES];
  // } Stop;
#endif
