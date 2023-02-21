#ifndef PARSE_ZJSON_H
#define PARSE_ZJSON_H
  #include <stdbool.h>
  #include <zephyr/data/json.h>
  #include <stop.h>

  int parse_json_for_stop(JSON_STOP *stop, char *json_string);
#endif