#ifdef CONFIG_STOP_REQUEST_BUSTRACKER
#include "parse_bustracker.h"

#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define JSMN_HEADER

#include "json/jsmn.h"
#include "json/json_helpers.h"

LOG_MODULE_REGISTER(parse_bustracker);

/** Convert BusTracker's route IDs to PVTA standard IDs */
static void *id_to_route_id(char rout_id[], int id) {
  switch (id) {
    case 30038:
      memcpy(rout_id, "38", sizeof("38"));
      break;
    case 10043:
      memcpy(rout_id, "B43", sizeof("B43"));
      break;
    case 10943:
      memcpy(rout_id, "B43E", sizeof("B43E"));
      break;
    case 20029:
      memcpy(rout_id, "R29", sizeof("R29"));
      break;
    case 30079:
      memcpy(rout_id, "B79", sizeof("B79"));
      break;
    default:
      rout_id[0] = '\0';
      break;
  }
  return NULL;
}

/** The number of keys + values in for each object type in our JSON string */
#define DEPARTURE_TOK_COUNT 18

#define HEADWAY_TOK_COUNT 14

#define ROUTE_DIRECTION_TOK_COUNT \
  (12 + (CONFIG_ROUTE_MAX_DEPARTURES * (DEPARTURE_TOK_COUNT + HEADWAY_TOK_COUNT)))

#define STOP_TOK_COUNT (6 + (CONFIG_STOP_MAX_ROUTES * ROUTE_DIRECTION_TOK_COUNT))

/** With jsmn JSON objects count as a token, so we need to offset by an
 *  additional 1 for each level deeper we go.
 *  These offsets need to be temporary though, because objects don't have a
 *  value to account for. At least that's what I believe the reason to be...
 */
// #define STOP_TOK tokens[t + 1]
#define ROUTE_DIRECTION_TOK tokens[t + rdir + 2]
#define DEPARTURE_TOK tokens[t + rdir + dep + 3]

static int unique_disply_text(
    const char *const json_ptr, RouteDirection *route_direction, const jsmntok_t *tok,
    size_t valid_departure_count
) {
  for (size_t i = 0; i < valid_departure_count; i++) {
    Departure *departure = &route_direction->departures[i];
    if (jsoneq(json_ptr, tok, departure->display_text)) {
      return 0;
    }
  }
  return 1;
}

/** Iterates through the Departures array objects to find desired values. */
static int parse_departures(
    const char *const json_ptr, int t, jsmntok_t tokens[], int rdir, size_t departures_count,
    RouteDirection *route_direction, const int time_now
) {
  size_t valid_departure_count = 0;

  for (int dep_num = 0; dep_num < departures_count; dep_num++) {
    Departure *departure = &route_direction->departures[valid_departure_count];
    int departure_uniq = 0;
    int edt_in_future = 0;

    /** Increase t by 1 to step into the object */
    t++;

    /** The number of keys in the Departure object.
     *
     *  ROUTE_DIRECTION_TOK is a Departure object,
     *  so ROUTE_DIRECTION_TOK.size is the number of Departure keys.
     */
    const size_t departure_size = ROUTE_DIRECTION_TOK.size;
    LOG_DBG(
        "***********Start Departure (t: %d, dep_num: %d, departure_size: "
        "%d)**********",
        t, dep_num, departure_size
    );
    for (int dep = 0; dep < (departure_size * 2); dep++) {
      if (jsoneq(json_ptr, &DEPARTURE_TOK, "DisplayText")) {
        dep++;
        departure_uniq =
            unique_disply_text(json_ptr, route_direction, &DEPARTURE_TOK, valid_departure_count);

        if (departure_uniq) {
          memcpy(
              departure->display_text, json_ptr + DEPARTURE_TOK.start,
              DEPARTURE_TOK.end - DEPARTURE_TOK.start
          );
          departure->display_text[(DEPARTURE_TOK.end - DEPARTURE_TOK.start)] = '\0';
          if (edt_in_future) {
            goto check_validity;
          }
        } else {
          break;
        }
      } else if (jsoneq(json_ptr, &DEPARTURE_TOK, "EDT")) {
        dep++;
        /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since
         * the epoch. We increase the pointer by 6 to remove the leading
         * '/Date(' for atoi(). This also ignores the timezone which we don't
         * need.
         */
        char edt_string[10];
        (void)memcpy(edt_string, json_ptr + DEPARTURE_TOK.start + 7, 10);

        // const char *const edt_string = json_ptr + DEPARTURE_TOK.start +
        // 7;
        unsigned int edt = atoi(edt_string);
        LOG_DBG("* EDT: %u", edt);
        if (edt > time_now) {
          departure->etd = edt;
          edt_in_future = true;
          if (departure_uniq) {
            goto check_validity;
          }
        } else if (departure_uniq) {
          departure->display_text[0] = '\0';
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
        LOG_WRN(
            "Unexpected key in Departures: %.*s\n", DEPARTURE_TOK.end - DEPARTURE_TOK.start,
            json_ptr + DEPARTURE_TOK.start
        );
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
  for (int i = 0; i < CONFIG_ROUTE_MAX_DEPARTURES; i++) {
    LOG_DBG("Uniq display text(s) %d: %s", i, route_direction->departures[i].display_text);
  }
#endif  // CONFIG_DEBUG
  route_direction->departures_size = valid_departure_count;
  return t;
}

/** Iterates through the RouteDirections array objects to find desired values.
 */
static int parse_route_directions(
    const char *const json_ptr, int t, jsmntok_t tokens[], size_t route_directions_count,
    Stop *stop, const int time_now
) {
  size_t valid_route_count = 0;

  for (int rd_num = 0; rd_num < route_directions_count; rd_num++) {
    RouteDirection *route_direction = &stop->route_directions[valid_route_count];
    /** The number of keys in the RouteDirection object.
     *
     *  tokens[t + 1] is a RouteDirection object,
     *  so tokens[t + 1].size is the number of RouteDirection keys.
     */
    const size_t route_direction_size = tokens[t + 1].size;
    LOG_DBG(
        "=======Start Route Direction (t: %d, Route_Direction_Size: %d, "
        "rd_num: %d)======",
        t, route_direction_size * 2, rd_num
    );
    /* jsmntok::size is the number of keys but we're iterating over keys and
     * values, so we need to double the size to get the true count.
     */
    for (int rdir = 0; rdir < (route_direction_size * 2); rdir++) {
      if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "Direction")) {
        rdir++;
        if ((*(json_ptr + ROUTE_DIRECTION_TOK.start) == 'N') ||
            (*(json_ptr + ROUTE_DIRECTION_TOK.start) == 'E')) {
          route_direction->direction_code = '0';
        } else {
          route_direction->direction_code = '1';
        }
        LOG_DBG("- Direction: %c", *(json_ptr + ROUTE_DIRECTION_TOK.start));
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "IsDone")) {
        rdir++;
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "IsHeadway")) {
        rdir++;
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "RouteId")) {
        rdir++;
        (void)id_to_route_id(route_direction->route_id, atoi(json_ptr + ROUTE_DIRECTION_TOK.start));
        LOG_DBG("- RouteId: %d", atoi(json_ptr + ROUTE_DIRECTION_TOK.start));
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "Departures")) {
        rdir++;
        if ((ROUTE_DIRECTION_TOK.type == JSMN_ARRAY) && (ROUTE_DIRECTION_TOK.size > 0)) {
          t = parse_departures(
              json_ptr, t, tokens, rdir, ROUTE_DIRECTION_TOK.size, route_direction, time_now
          );
          valid_route_count++;
        } else {
          break;
        }
      } else if (jsoneq(json_ptr, &ROUTE_DIRECTION_TOK, "HeadwayDepartures")) {
        rdir++;
        if ((ROUTE_DIRECTION_TOK.type == JSMN_ARRAY) && (ROUTE_DIRECTION_TOK.size > 0)) {
          /* We don't care about this array but we do need to account for it's
           * size */
          t += (ROUTE_DIRECTION_TOK.size * 2) + 1;
        }
      } else {
        /* We don't care about other keys so skip their values */
        rdir++;
        LOG_WRN(
            "Unexpected key in RouteDirections: %.*s\n",
            ROUTE_DIRECTION_TOK.end - ROUTE_DIRECTION_TOK.start,
            json_ptr + ROUTE_DIRECTION_TOK.start
        );
      }
    }
    /* Increase t by an additional 1 to step into the next object */
    t += (route_direction_size * 2) + 1;
    LOG_DBG(
        "==============================(t: %d, rd_num: "
        "%d)========================\n",
        t, rd_num
    );
  }
  stop->routes_size = valid_route_count;
  return t;
}

/** Parses a JSON string with jsmn, then iterates through the jsmn key tokens
 *  to find desired values.
 *
 * TODO: Acount for size of *optional* arrays while iterating.
 */
int parse_bustracker_json(const char *const json_ptr, Stop *stop, unsigned int time_now) {
  jsmn_parser p;

  /** The number of maximum possible tokens we expect in our JSON string + 1 for
   * the \0 delimiter. */
  jsmntok_t tokens[STOP_TOK_COUNT + 1];

  /** The jsmn token counter. */
  int t;
  int ret;
  int err;

  jsmn_init(&p);

  /** The number of tokens *allocated* from tokens array to parse the JSON
   * string */
  ret = jsmn_parse(&p, json_ptr, strlen(json_ptr), tokens, sizeof(tokens) / sizeof(jsmntok_t));

  err = eval_jsmn_return(ret);
  if (err) {
    LOG_ERR("Failed to parse JSON");
    return err;
  }

  LOG_INF("Tokens allocated: %d/%d\n", ret, STOP_TOK_COUNT);

  if (ret < 2) {
    LOG_INF("No scheduled departures");
    return 5;
  }

  /* Set the starting position for t */
  switch (tokens[0].type) {
    case JSMN_ARRAY:
      /* If the root token is an array, we assume the next token is an object;
       * t is set to 2 so we can start in the object.
       */
      t = 2;
      break;
    case JSMN_OBJECT:
      /* If the root token an object t is set to 1 so we can start in the
       * object. */
      t = 1;
      break;
    default:
      LOG_ERR("Top level token isn't an array or object.");
      return EXIT_FAILURE;
  }

  /* We want to loop over all the keys of the root object.
   * We know the token after a key is a value so we skip that iteration.
   */
  while (t < ret) {
    if (jsoneq(json_ptr, &tokens[t], "LastUpdated")) {
      t++;
    } else if (jsoneq(json_ptr, &tokens[t], "RouteDirections")) {
      /* Increase t by an additional 1 to step into the array */
      t++;
      /* tokens[t] is the RouteDirections array in this case,
       * so tokens[t].size is the number of RouteDirections
       */
      if (tokens[t].type == JSMN_ARRAY && tokens[t].size > 0) {
        t = parse_route_directions(json_ptr, t, tokens, tokens[t].size, stop, time_now);
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
      LOG_WRN(
          "Unexpected key in Stop: %.*s\n", tokens[t].end - tokens[t].start,
          json_ptr + tokens[t].start
      );
    }
    t++;
  }
  return EXIT_SUCCESS;
}
#endif  // CONFIG_STOP_REQUEST_BUSTRACKER
