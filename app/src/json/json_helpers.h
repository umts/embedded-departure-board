/** @file json_helpers.h
 *  @brief Macro and function defines for the NIDD client.
 */

#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

#define JSMN_HEADER

#include "json/jsmn.h"

/** Compares a string with a jsmn token value. */
const int jsoneq(const char *const json_ptr, const jsmntok_t *tok, const char *const string);

const int eval_jsmn_return(const int ret);
#endif
