/** @headerfile json_helpers.h */
#include "json/json_helpers.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(json_helpers);

/** Compares a string with a jsmn token value. */
const int jsoneq(const char *const json_ptr, const jsmntok_t *tok, const char *const string) {
  if (tok->type == JSMN_STRING && (int)strlen(string) == tok->end - tok->start &&
      strncmp(json_ptr + tok->start, string, tok->end - tok->start) == 0) {
    return 1;
  }
  return 0;
}

const int eval_jsmn_return(const int ret) {
  switch (ret) {
    case JSMN_ERROR_NOMEM:
      LOG_ERR("Failed to parse JSON; Not enough tokens were provided.\n");
      return 1;
    case JSMN_ERROR_INVAL:
      LOG_ERR("Failed to parse JSON; Invalid character inside JSON string.\n");
      return 2;
    case JSMN_ERROR_PART:
      LOG_ERR(
          "Failed to parse JSON; The string is not a full JSON packet, more "
          "bytes expected.\n"
      );
      return 3;
    case 0:
      LOG_WRN("Parsed Empty JSON string.\n");
      return 4;
    default:
      return 0;
  }
}
