#include <jsmn_parse.h>
#include <jsmn.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>

/* Newlib C includes */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

/* app includes */
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(jsmn_parse, LOG_LEVEL_INF);

/** The number of keys + values in for each object type in our JSON string */
#define DEPARTURE_TOK_COUNT 18
#define HEADWAY_TOK_COUNT 14
#define ROUTE_DIRECTION_TOK_COUNT \
  (12 + (MAX_DEPARTURES * (DEPARTURE_TOK_COUNT + HEADWAY_TOK_COUNT)))
#define STOP_TOK_COUNT (6 + (MAX_ROUTES * ROUTE_DIRECTION_TOK_COUNT))

/** With jsmn JSON objects count as a token, so we need to offset by an
 *  additional 1 for each level deeper we go.
 *  These offsets need to be temporary though, because objects don't have a
 *  value to account for. At least that's what I believe the reason to be...
 */
// #define STOP_TOK tokens[t + 1]
#define ROUTE_DIRECTION_TOK tokens[t + rdir + 2]
#define DEPARTURE_TOK tokens[t + rdir + dep + 3]

static jsmn_parser p;

/** The number of maximum possible tokens we expect in our JSON string + 1 for the \0 delimiter. */
static jsmntok_t tokens[STOP_TOK_COUNT + 1];

/** The jsmn token counter. */
static unsigned int t;

/** Compares a string with a jsmn token value. */
static bool jsoneq(const char *json_ptr, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json_ptr + tok->start, s, tok->end - tok->start) == 0) {
    return true;
  }
  return false;
}

#ifdef CONFIG_DEBUG
static void print_token_substring(const char *json_ptr, jsmntok_t *token) {
  if (token->type == JSMN_STRING) {
    LOG_DBG("Token: %.*s", token->end - token->start, json_ptr + token->start);
  } else if (token->type == JSMN_PRIMITIVE) {
    switch (*(json_ptr + token->start)) {
    case 'n':
      LOG_DBG("Token is NULL");
      break;
    case 't':
      LOG_DBG("Token is TRUE");
      break;
    case 'f':
      LOG_DBG("Token is FALSE");
      break;
    default:
      LOG_DBG("Token: %d", atoi(json_ptr + token->start));
      break;
    }
  } else {
    LOG_DBG("Token: %.*s, size: %d, type: %d", token->end - token->start, json_ptr + token->start,
            token->size, token->type);
  }
}
#endif

bool unique_disply_text(const char *json_ptr, char array[][50], jsmntok_t *tok, size_t arr_len) {
  for (int i = 0; i < arr_len; i++) {
    if (jsoneq(json_ptr, tok, array[i])) {
      return false;
    }
  }
  return true;
}

/** Iterates through the Departures array objects to find desired values. */
static void parse_departures(char *json_ptr, int rdir, const size_t departures_count,
                             RouteDirection *route_direction, int time_now) {
  static char unique_deps[MAX_DEPARTURES][50];
  size_t valid_departure_count = 0;

  for (int dep_num = 0; dep_num < departures_count; dep_num++) {
    Departure *departure = &route_direction->departures[valid_departure_count];
    bool departure_uniq = false;
    bool edt_in_future = false;

    /** Increase t by 1 to step into the object */
    t++;

    /** The number of keys in the Departure object.
     *
     *  ROUTE_DIRECTION_TOK is a Departure object,
     *  so ROUTE_DIRECTION_TOK.size is the number of Departure keys.
     */
    const size_t departure_size = ROUTE_DIRECTION_TOK.size;
    LOG_DBG("***********Start Departure (t: %d, dep_num: %d, departure_size: %d)**********", t,
            dep_num, departure_size);
    for (int dep = 0; dep < (departure_size * 2); dep++) {
      if (jsoneq(json_ptr, &DEPARTURE_TOK, "DisplayText")) {
        dep++;
        LOG_DBG("* DisplayText:");
        print_token_substring(json_ptr, &DEPARTURE_TOK);
        departure_uniq =
            unique_disply_text(json_ptr, unique_deps, &DEPARTURE_TOK, valid_departure_count);
        if (departure_uniq) {
          strncpy(unique_deps[valid_departure_count], json_ptr + DEPARTURE_TOK.start,
                  DEPARTURE_TOK.end - DEPARTURE_TOK.start);
          unique_deps[valid_departure_count][(DEPARTURE_TOK.end - DEPARTURE_TOK.start)] = '\0';
          if (edt_in_future) {
            goto check_validity;
          }
        } else {
          break;
        }
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "EDT")) {
        dep++;
        /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
         * We increase the pointer by 6 to remove the leading '/Date(' for atoi().
         * This also ignores the timezone which we don't need.
         */
        char *edt_string = json_ptr + DEPARTURE_TOK.start + 7;
        edt_string[10] = '\0';
        unsigned int edt = atoi(edt_string);
        LOG_DBG("* EDT: %d", edt);
        if (edt > time_now) {
          departure->etd = edt;
          edt_in_future = true;
          if (departure_uniq) {
            goto check_validity;
          }
        } else if (departure_uniq) {
          memset(unique_deps[valid_departure_count], '\0', sizeof(unique_deps[0]));
          break;
        } else {
          break;
        }
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "ETA")) {
        dep++;
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "GoogleTripId")) {
        dep++;
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "LastUpdated")) {
        dep++;
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "SDT")) {
        dep++;
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "STA")) {
        dep++;
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "VehicleId")) {
        dep++;
      } else {
        dep++;
        LOG_WRN("Unexpected key in Departures: %.*s\n", DEPARTURE_TOK.end - DEPARTURE_TOK.start,
                json_ptr + DEPARTURE_TOK.start);
      }

    check_validity:
      if (departure_uniq && edt_in_future) {
        valid_departure_count++;
        break;
      }
    }
    t += (departure_size * 2);
    LOG_DBG("*********************(t: %d)********************\n", t);
  }
#ifdef CONFIG_DEBUG
  for (int i = 0; i < MAX_DEPARTURES; i++) {
    LOG_DBG("Uniq display text(s) %d: %s", i, unique_deps[i]);
  }
#endif
  route_direction->departures_size = valid_departure_count;
}

/** Iterates through the RouteDirections array objects to find desired values. */
static void parse_route_directions(char *json_ptr, const size_t route_directions_count, Stop *stop,
                                   int time_now) {
  size_t valid_route_count = 0;

  for (int rd_num = 0; rd_num < route_directions_count; rd_num++) {
    RouteDirection *route_direction = &stop->route_directions[valid_route_count];
    /** The number of keys in the RouteDirection object.
     *
     *  tokens[t + 1] is a RouteDirection object,
     *  so tokens[t + 1].size is the number of RouteDirection keys.
     */
    const size_t route_direction_size = tokens[t + 1].size;
    LOG_DBG("=======Start Route Direction (t: %d, Route_Direction_Size: %d, rd_num: %d)======", t,
            route_direction_size * 2, rd_num);
    /* jsmntok::size is the number of keys but we're iterating over keys and values,
     * so we need to double the size to get the true count.
     */
    for (int rdir = 0; rdir < (route_direction_size * 2); rdir++) {
      if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "Direction")) {
        rdir++;
        route_direction->direction_code = *(json_ptr + ROUTE_DIRECTION_TOK.start);
        LOG_DBG("- Direction: %c", *(json_ptr + ROUTE_DIRECTION_TOK.start));
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "IsDone")) {
        rdir++;
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "IsHeadway")) {
        rdir++;
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "RouteId")) {
        rdir++;
        route_direction->id = atoi(json_ptr + ROUTE_DIRECTION_TOK.start);
        LOG_DBG("- RouteId: %d", atoi(json_ptr + ROUTE_DIRECTION_TOK.start));
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "Departures")) {
        rdir++;
        if ((ROUTE_DIRECTION_TOK.type == JSMN_ARRAY) && (ROUTE_DIRECTION_TOK.size > 0)) {
          parse_departures(json_ptr, rdir, ROUTE_DIRECTION_TOK.size, route_direction, time_now);
          valid_route_count++;
        } else {
          break;
        }
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "HeadwayDepartures")) {
        rdir++;
        if ((ROUTE_DIRECTION_TOK.type == JSMN_ARRAY) && (ROUTE_DIRECTION_TOK.size > 0)) {
          /* We don't care about this array but we do need to account for it's size */
          t += (ROUTE_DIRECTION_TOK.size * 2) + 1;
        }
      } else {
        /* We don't care about other keys so skip their values */
        rdir++;
        LOG_WRN("Unexpected key in RouteDirections: %.*s\n",
                ROUTE_DIRECTION_TOK.end - ROUTE_DIRECTION_TOK.start,
                json_ptr + ROUTE_DIRECTION_TOK.start);
      }
    }
    /* Increase t by an additional 1 to step into the next object */
    t += (route_direction_size * 2) + 1;
    LOG_DBG("==============================(t: %d, rd_num: %d)========================\n", t,
            rd_num);
  }
  stop->routes_size = valid_route_count;
}

/** Parses a JSON string with jsmn, then iterates through the jsmn key tokens
 *  to find desired values.
 *
 * TODO: Acount for size of *optional* arrays while iterating.
 */
int parse_json_for_stop(char *json_ptr, Stop *stop) {
  jsmn_init(&p);

  /** The number of tokens *allocated* from tokens array to parse the JSON string */
  const int ret =
      jsmn_parse(&p, json_ptr, strlen(json_ptr), tokens, sizeof(tokens) / sizeof(jsmntok_t));

  if (ret < 0) {
    LOG_ERR("Failed to parse JSON: %d.", ret);
    return EXIT_FAILURE;
  } else if (ret < 1) {
    LOG_ERR("Parsed Empty JSON string.");
    return EXIT_FAILURE;
  }
  LOG_INF("Tokens allocated: %d/%d\n", ret, STOP_TOK_COUNT);

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
    return EXIT_FAILURE;
  }

  int time_now = get_rtc_time();
  if (time_now == -1) {
    return 2;
  }

  /* We want to loop over all the keys of the root object.
   * We know the token after a key is a value so we skip that iteration.
   */
  while (t < ret) {
    if (jsoneq(json_ptr, &tokens[t], "LastUpdated")) {
      t++;
      char *last_updated_string = json_ptr + tokens[t].start + 7;
      // last_updated_string[10] = '\0';
      LOG_DBG("LastUpdated: %lld\n", strtoll(last_updated_string, NULL, 10));
      unsigned long long new_last_updated = strtoll(last_updated_string, NULL, 10);
      if (stop->last_updated < new_last_updated) {
        stop->last_updated = new_last_updated;
      } else {
        LOG_INF("StopDepartures not updated, skipping.");
        break;
      }
    } else if (jsoneq(json_ptr, &tokens[t], "RouteDirections")) {
      /* Increase t by an additional 1 to step into the array */
      t++;
      /* tokens[t] is the RouteDirections array in this case,
       * so tokens[t].size is the number of RouteDirections
       */
      if (tokens[t].type == JSMN_ARRAY && tokens[t].size > 0) {
        parse_route_directions(json_ptr, tokens[t].size, stop, time_now);
      } else {
        LOG_WRN("No RouteDirections to parse.");
        break;
      }
    } else if (jsoneq(json_ptr, &tokens[t], "StopId")) {
      t++;
      /** TODO: Maybe verify the ID matches the one requested? Seems silly. */
      LOG_DBG("StopId: %d\n", atoi(json_ptr + tokens[t].start));
    } else {
      t++;
      LOG_WRN("Unexpected key in Stop: %.*s\n", tokens[t].end - tokens[t].start,
              json_ptr + tokens[t].start);
    }
    t++;
  }
  return EXIT_SUCCESS;
}
