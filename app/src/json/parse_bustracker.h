#ifndef PARSE_BUSTRACKER_H
#define PARSE_BUSTRACKER_H
#include "stop.h"

int parse_bustracker_json(const char *const json_ptr, Stop *stop, unsigned int time_now);
#endif  // PARSE_BUSTRACKER_H
