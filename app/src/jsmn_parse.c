#include <jsmn_parse.h>
#include <jsmn.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Newlib C includes */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

/* app includes */
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(jsmn_parse, LOG_LEVEL_DBG);

/** The number of keys + values in for each object type in our JSON string */
#define TRIP_TOK_COUNT 26
#define DEPARTURE_TOK_COUNT (52 + TRIP_TOK_COUNT)
#define ROUTE_DIRECTION_TOK_COUNT (18 + (MAX_DEPARTURES * DEPARTURE_TOK_COUNT))
#define STOP_TOK_COUNT (8 + (MAX_ROUTES * ROUTE_DIRECTION_TOK_COUNT))

/** With jsmn JSON objects count as a token, so we need to offset by an
 *  additional 1 for each level deeper we go.
 *  These offsets need to be temporary though, because objects don't have a
 *  value to account for. At least that's what I believe the reason to be...
 */
// #define STOP_TOK tokens[t + 1]
#define ROUTE_DIRECTION_TOK tokens[t + rdir + 2]
#define DEPARTURE_TOK tokens[t + rdir + dep + 3]
#define TRIP_TOK tokens[t + rdir + dep + trip + 4]

static jsmn_parser p;

/** The number of maximum possible tokens we expect in our JSON string + 1 for the \0 delimiter. */
static jsmntok_t tokens[STOP_TOK_COUNT + 1];

/** The jsmn token counter. */
static unsigned int t;

/** Compares a string with a jsmn token value. */
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

static void extract_token_substring(const char *json_ptr, jsmntok_t *token) {
  if (token->type == JSMN_STRING) {
    LOG_INF("Token: %.*s", token->end - token->start, json_ptr + token->start);
  } else if (token->type == JSMN_PRIMITIVE) {
    switch (*(json_ptr + token->start)) {
    case 'n':
      LOG_INF("Token is NULL");
      break;
    case 't':
      LOG_INF("Token is TRUE");
      break;
    case 'f':
      LOG_INF("Token is FALSE");
      break;
    default:
      LOG_INF("Token: %d", atoi(json_ptr + token->start));
      break;
    }
  } else {
    LOG_INF("Token: %.*s, size: %d, type: %d", token->end - token->start,
      json_ptr + token->start, token->size, token->type);
  }

}

/** Iterates through the Trip object keys to find desired values. */
static void parse_trip(char *json_ptr, int rdir, int dep, const size_t trip_size) {
  LOG_DBG(">>>>>>>>>Trip (t: %d, dep: %d, trip_size: %d)<<<<<<<<<", t, dep, trip_size);
  for (int trip = 0; trip < trip_size * 2; trip++) {
    if (jsoneq(json_ptr, &TRIP_TOK, "InternetServiceDesc") == 0) {
      LOG_INF("> InternetServiceDesc:");
      extract_token_substring(json_ptr, &TRIP_TOK);
      trip++;
    } else if (jsoneq(json_ptr, &TRIP_TOK, "TripDirection") == 0) {
      LOG_INF("> TripDirection:");
      extract_token_substring(json_ptr, &TRIP_TOK);
      trip++;
    } else {
      /* We don't care about other keys so skip their values */
      trip++;
    }
  }
  t += (trip_size * 2);
  LOG_DBG(">>>>>>>>>>>>>>>>>(t: %d, trip_size: %d)<<<<<<<<<<<<<<<", t, trip_size);
}

/** Iterates through the Departures array objects to find desired values. */
static void parse_departures(char *json_ptr, int rdir, const size_t departures_count) {
  for (int dep_num = 0; dep_num < departures_count; dep_num++) {
    /** Increase t by 1 to step into the object */
    t++;

    /** The number of keys in the Departure object.
     *
     *  ROUTE_DIRECTION_TOK is a Departure object,
     *  so ROUTE_DIRECTION_TOK.size is the number of Departure keys.
     */
    const size_t departure_size = ROUTE_DIRECTION_TOK.size;
    LOG_DBG("***********Start Departure (t: %d, dep_num: %d, departure_size: %d)**********", t, dep_num, departure_size);
    for (int dep = 0; dep < (departure_size * 2); dep++) {
      if (jsoneq(json_ptr, &DEPARTURE_TOK, "EDT") == 0) {
        dep++;
        LOG_INF("* EDT: %d", atoi(json_ptr + DEPARTURE_TOK.start));

      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "StopStatusReportLabel") == 0) {
        dep++;
        LOG_INF("* StopStatusReportLabel:");
        extract_token_substring(json_ptr, &DEPARTURE_TOK);
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "Trip") == 0) {
        dep++;
        if (DEPARTURE_TOK.type == JSMN_OBJECT && DEPARTURE_TOK.size != 0) {
          /* DEPARTURE_TOK is a Trip object in this case,
           * so DEPARTURE_TOK.size is the number of Trip keys.
           */
          parse_trip(json_ptr, rdir, dep, DEPARTURE_TOK.size);
        }
      } else {
        /* We don't care about other keys so skip their values */
        dep++;
      }
    }
    t += (departure_size * 2);
    LOG_DBG("*********************(t: %d)********************\n", t);
  }
}

/** Iterates through the RouteDirections array objects to find desired values. */
static void parse_route_directions(char *json_ptr, const size_t route_directions_count) {
  for (int rd_num = 0; rd_num < route_directions_count; rd_num++) {
    /** The number of keys in the RouteDirection object.
     *
     *  tokens[t + 1] is a RouteDirection object,
     *  so tokens[t + 1].size is the number of RouteDirection keys.
     */
    const size_t route_direction_size = tokens[t + 1].size;
    LOG_DBG("=======Start Route Direction (t: %d, Route_Direction_Size: %d, rd_num: %d)======", t, route_direction_size * 2, rd_num);
    /* jsmntok::size is the number of keys but we're iterating over keys and values,
     * so we need to double the size to get the true count.
     */
    for (int rdir = 0; rdir < (route_direction_size * 2); rdir++) {
      if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "Departures") == 0) {
        rdir++;
        if ((ROUTE_DIRECTION_TOK.type == JSMN_ARRAY) && (ROUTE_DIRECTION_TOK.size > 0)) {
          parse_departures(json_ptr, rdir, ROUTE_DIRECTION_TOK.size);
        }
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "DirectionCode") == 0) {
        rdir++;
        LOG_INF("- DirectionCode: %c", *(json_ptr + ROUTE_DIRECTION_TOK.start));
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "RouteId") == 0) {
        rdir++;
        LOG_INF("- RouteId: %d", atoi(json_ptr + ROUTE_DIRECTION_TOK.start));
      } else {
        /* We don't care about other keys so skip their values */
        rdir++;
      }
    }
    /* Increase t by an additional 1 to step into the next object */
    t += (route_direction_size * 2) + 1;
    LOG_DBG("==============================(t: %d, rd_num: %d)========================\n", t, rd_num);
  }
}

/** Parses a JSON string with jsmn, then iterates through the jsmn key tokens
 *  to find desired values.
 *
 * TODO: Acount for size of *optional* arrays while iterating.
 */
int parse_json_for_stop(char *json_ptr) {
  jsmn_init(&p);

  /** The number of tokens *allocated* from tokens array to parse the JSON string */
  const int ret = jsmn_parse(&p, json_ptr, strlen(json_ptr), tokens, sizeof(tokens) / sizeof(jsmntok_t));

  if (ret < 0) {
    LOG_ERR("Failed to parse JSON: %d.", ret);
    return 1;
  } else if (ret < 1) {
    LOG_ERR("Parsed Empty JSON string.");
    return 1;
  }

  LOG_INF("Number of allocated tokens: %d\n", ret);

  /* Set the starting position for t */
  switch (tokens[0].type) {
  case JSMN_ARRAY:
    /* If the root token is an array, we assume the next token is an object;
     * t is set to 2 so we can start in the object.
     */
    t = 2;
    break;
  case JSMN_OBJECT:
    /* If the root token an object t is set to 1 so we can start in the object. */
    t = 1;
    break;
  default:
    LOG_ERR("Top level token isn't an array or object.");
    return 1;
  }

  /* We want to loop over all the keys of the root object.
   * We know the token after a key is a value so we skip that iteration.
   */
  while (t < ret) {
    if (jsoneq(json_ptr, &tokens[t], "LastUpdated") == 0) {
      t++;
      LOG_INF("LastUpdated: %.*s\n", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start);
    } else if (jsoneq(json_ptr, &tokens[t], "RouteDirections") == 0) {
      /* Increase t by an additional 1 to step into the array */
      t++;
      /* tokens[t] is the RouteDirections array in this case,
       * so tokens[t].size is the number of RouteDirections
       */
      if (tokens[t].type == JSMN_ARRAY && tokens[t].size > 0) {
        parse_route_directions(json_ptr, tokens[t].size);
      }
    } else if (jsoneq(json_ptr, &tokens[t], "StopId") == 0) {
      t++;
      LOG_INF("StopId: %d\n", atoi(json_ptr + tokens[t].start));
    } else if (jsoneq(json_ptr, &tokens[t], "StopRecordId") == 0) {
      /* We don't actually care about this value */
      t++;
    } else {
      t++;
      LOG_WRN("Unexpected key: %.*s\n", tokens[t].end - tokens[t].start,
        json_ptr + tokens[t].start);
      break;
    }
    t++;
  }
  return EXIT_SUCCESS;
}
