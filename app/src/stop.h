#ifndef STOPS_H
#define STOPS_H

  /* Edit this to chose what stop you want to query */
  #define STOP_ID "73"

  /* Specify what routes you want to get departure times for, leave blank for all routes */
  #define ROUTE_IDS []

  #include <zephyr/sys/timeutil.h>
  #include <inttypes.h>
  #include <stdbool.h>

  struct Departure {
    int id;
    bool skipped;
    char *isd;
    time_t etd;
    time_t std;
  };

  struct Route {
    char direction;
    int id;
    int departures_size;
    struct Departure departures[];
  };
  
  struct Stop {
    const char *id;
    int routes_size;
    struct Route *routes[];
  };
#endif