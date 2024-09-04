/** @headerfile json_helpers.h */
#include "json/json_helpers.h"

/** Compares a string with a jsmn token value. */
extern inline int jsoneq(
    const char *const json_ptr, const jsmntok_t *tok, const char *const string
);

extern inline int eval_jsmn_return(int ret);
