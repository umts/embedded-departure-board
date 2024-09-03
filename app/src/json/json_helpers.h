/** @file json_helpers.h
 *  @brief Macro and function defines for the NIDD client.
 */

#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

#define JSMN_HEADER

#include <string.h>
#include <zephyr/kernel.h>

#include "json/jsmn.h"

/** Compares a string with a jsmn token value. */
inline int jsoneq(
    const char *const json_ptr, const jsmntok_t *tok, const char *const string
) {
  if (tok->type == JSMN_STRING &&
      (int)strlen(string) == tok->end - tok->start &&
      strncmp(json_ptr + tok->start, string, tok->end - tok->start) == 0) {
    return 1;
  }
  return 0;
}

inline int eval_jsmn_return(int ret) {
  switch (ret) {
    case 0:
      printk("Parsed Empty JSON string.\n");
      return 1;
    case JSMN_ERROR_NOMEM:
      printk("Failed to parse JSON; Not enough tokens were provided.\n");
      return 1;
    case JSMN_ERROR_INVAL:
      printk("Failed to parse JSON; Invalid character inside JSON string.\n");
      return 1;
    case JSMN_ERROR_PART:
      printk(
          "Failed to parse JSON; The string is not a full JSON packet, more "
          "bytes expected.\n"
      );
      return 1;
    default:
      // printk("Tokens allocated: %d/%d\n", ret, 8);
      return 0;
  }
}
#endif
