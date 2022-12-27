#ifndef ROUTES_H
#define ROUTES_H

  #include <zephyr/sys/timeutil.h>

  struct Routes {
    const char *name;
    const int id;
    // North, South/East, West
    time_t etd[2];
  };
#endif