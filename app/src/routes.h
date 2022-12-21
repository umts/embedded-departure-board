#ifndef ROUTES_H
#define ROUTES_H
  struct Routes {
    const char *name;
    const int id;
    // North, South/East, West
    int etd[2];
  };
#endif