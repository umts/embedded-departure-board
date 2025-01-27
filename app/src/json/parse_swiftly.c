#ifdef CONFIG_STOP_REQUEST_SWIFTLY
#include "parse_swiftly.h"

#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define JSMN_HEADER

#include "json/jsmn.h"
#include "json/json_helpers.h"

LOG_MODULE_REGISTER(parse_swiftly);

/** Total expected tokens for "predictions" array
 * 8 key tokens + 8 value tokens + 1 object token + 1 array token = 18 tokens
 */
#define PREDICTIONS_TOK_COUNT 18

/** Total expected tokens for "destinations" array
 * 3 key tokens + 3 value tokens + 1 object token + 1 array token = 8 tokens
 */
#define DESTINATION_TOK_COUNT 8

/** Total expected tokens for "predictionsData" array
 * 7 key tokens + 7 value tokens = 14 tokens
 */
#define PREDICTIONS_DATA_TOK_COUNT \
  (14 + (CONFIG_ROUTE_MAX_DEPARTURES * (DESTINATION_TOK_COUNT + PREDICTIONS_TOK_COUNT)))

/** Total expected tokens for requested stop JSON
 * 3 key tokens + 3 value tokens = 6 tokens
 */
#define STOP_TOK_COUNT (6 + (CONFIG_STOP_MAX_ROUTES * PREDICTIONS_DATA_TOK_COUNT))

/** Iterates through the predictions array objects to find desired values. */
static int parse_predictions(
    const char *const json_ptr, int t, jsmntok_t tokens[], size_t array_size,
    Destination *destination, const int time_now
) {
  /** The number of key-value pairs in a prediction object */
  int prediction_size;

  /* tokens[t] is the predictions array, array_size == tokens[t].size (the number of objects
   * in the array), tokens[t + 1] is the first object.
   */
  t++;

  for (int pred_count = 0; pred_count < array_size; pred_count++) {
    // tokens[t].size is the number of key-value pairs in the object
    prediction_size = tokens[t].size;

    // tokens[t] is a prediction object, tokens[t + 1] is the first key
    t++;

    LOG_DBG("              {");
    for (int pred = 0; pred < prediction_size; pred++) {
      if (jsoneq(json_ptr, &tokens[t], "time")) {
        t++;
        LOG_DBG(
            "                time: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "sec")) {
        t++;
        LOG_DBG(
            "                sec: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "min")) {
        t++;
        destination->min = atoi(json_ptr + tokens[t].start);
        LOG_DBG(
            "                min: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "departure")) {
        t++;
        LOG_DBG(
            "                departure: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "blockId")) {
        t++;
        LOG_DBG(
            "                blockId: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "occupancyStatus")) {
        t++;
        LOG_DBG(
            "                occupancyStatus: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "vehicleId")) {
        t++;
        LOG_DBG(
            "                vehicleId: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "tripId")) {
        t++;
        LOG_DBG(
            "                tripId: %.*s", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else {
        LOG_WRN(
            "Unexpected key in predictions: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
        // We don't expect other keys, so skip their values
        t++;
      }
      // Increment 1 to get to next key
      t++;
    }
    LOG_DBG("              },");
    // Increment 1 to get to next object
    t++;
  }
  /* Because we're looping over key-value pairs we increment t by 1 for each match to get the
   * value, then at the end of the loop we increment by another 1 to get the next key. JSON arrays
   * are tokens, but they aren't key-value. However, we already incremented t as if was a key-value
   * pair. So, we need to decrement t by the array depth we're leaving.
   */
  return t - 3;
}

/** Iterates through the destinations array objects to find desired values. */
static int parse_destinations(
    const char *const json_ptr, int t, jsmntok_t tokens[], size_t array_size,
    PredictionsData *predictions_data, const int time_now
) {
  /** The number of key-value pairs in a destination object */
  int destination_size;

  predictions_data->destinations_size = array_size;

  /* tokens[t] is the destinations array, array_size == tokens[t].size (the number of objects
   * in the array), tokens[t + 1] is the first object.
   */
  t++;

  for (int dest_count = 0; dest_count < array_size; dest_count++) {
    Destination *destination = &predictions_data->destinations[dest_count];

    // tokens[t].size is the number of key-value pairs in the object
    destination_size = tokens[t].size;

    // tokens[t] is a destination object, tokens[t + 1] is the first key
    t++;

    LOG_DBG("          {");
    for (int dest = 0; dest < destination_size; dest++) {
      if (jsoneq(json_ptr, &tokens[t], "directionId")) {
        t++;
        destination->direction_id = *(json_ptr + tokens[t].start);
        LOG_DBG(
            "            directionId: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "headsign")) {
        t++;
        LOG_DBG(
            "            headsign: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "predictions")) {
        LOG_DBG("            predictions: [");
        t++;
        if ((tokens[t].type == JSMN_ARRAY) && (tokens[t].size > 0)) {
          t = parse_predictions(json_ptr, t, tokens, tokens[t].size, destination, time_now);
        } else {
          break;
        }
        LOG_DBG("            ]");
      } else {
        LOG_WRN(
            "Unexpected key in destinations: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
        // We don't expect other keys, so skip their values
        t++;
      }
      // Increment 1 to get to next key
      t++;
    }
    LOG_DBG("          },");
    // Increment 1 to get to next object
    t++;
  }

  /* Because we're looping over key-value pairs we increment t by 1 for each match to get the
   * value, then at the end of the loop we increment by another 1 to get the next key. JSON arrays
   * are tokens, but they aren't key-value. However, we already incremented t as if was a key-value
   * pair. So, we need to decrement t by the array depth we're leaving
   */
  return t - 2;
}

/** Iterates through the predictionsData array objects to find desired values. */
static int parse_predictions_data(
    const char *const json_ptr, int t, jsmntok_t tokens[], size_t array_size, Stop *stop,
    const int time_now
) {
  /** The number of key-value pairs in a predictionsData object */
  int pdata_size;

  stop->routes_size = array_size;

  /* tokens[t] is the predictionsData array, array_size == tokens[t].size (the number of objects
   * in the array), tokens[t + 1] is the first object.
   */
  t++;

  for (int pdata_count = 0; pdata_count < array_size; pdata_count++) {
    PredictionsData *predictions_data = &stop->predictions_data[pdata_count];

    // tokens[t].size is the number of key-value pairs in the object
    pdata_size = tokens[t].size;

    // tokens[t] is a predictionsData object, tokens[t + 1] is the first key
    t++;

    LOG_DBG("      {");
    for (int pdata = 0; pdata < pdata_size; pdata++) {
      if (jsoneq(json_ptr, &tokens[t], "routeShortName")) {
        t++;
        LOG_DBG(
            "        routeShortName: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "routeName")) {
        t++;
        LOG_DBG(
            "        routeName: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "routeId")) {
        t++;
        LOG_DBG(
            "        routeId: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start
        );
        memcpy(
            predictions_data->route_id, json_ptr + tokens[t].start, tokens[t].end - tokens[t].start
        );
        predictions_data->route_id[(tokens[t].end - tokens[t].start)] = '\0';
      } else if (jsoneq(json_ptr, &tokens[t], "stopId")) {
        t++;
        LOG_DBG(
            "        stopId: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "stopName")) {
        t++;
        LOG_DBG(
            "        stopName: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "routeName")) {
        t++;
        LOG_DBG(
            "        routeName: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "stopCode")) {
        t++;
        LOG_DBG(
            "        stopCode: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start
        );
      } else if (jsoneq(json_ptr, &tokens[t], "destinations")) {
        LOG_DBG("        destinations: [");
        t++;
        if ((tokens[t].type == JSMN_ARRAY) && (tokens[t].size > 0)) {
          t = parse_destinations(json_ptr, t, tokens, tokens[t].size, predictions_data, time_now);
        } else {
          break;
        }
        LOG_DBG("        ]");
      } else {
        LOG_WRN(
            "Unexpected key in predictionsData: %.*s,", tokens[t].end - tokens[t].start,
            json_ptr + tokens[t].start
        );
        // We don't expect other keys, so skip their values
        t++;
      }
      // Increment 1 to get to next key
      t++;
    }
    LOG_DBG("      },");
    // Increment 1 to get to next object
    t++;
  }
  return t;
}

/** Iterates through the data array objects to find desired values. */
static int parse_data(
    const char *const json_ptr, int t, jsmntok_t tokens[], size_t data_size, Stop *stop,
    const int time_now
) {
  /* tokens[t] is the data object, data_size == tokens[t].size (the number of key-value pairs in the
   * object), tokens[t + 1] is the first key.
   */
  t++;

  for (int data = 0; data < data_size; data++) {
    if (jsoneq(json_ptr, &tokens[t], "agencyKey")) {
      t++;
      LOG_DBG("    agencyKey: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start);
    } else if (jsoneq(json_ptr, &tokens[t], "predictionsData")) {
      t++;
      if ((tokens[t].type == JSMN_ARRAY) && (tokens[t].size > 0)) {
        LOG_DBG("    predictionsData: [");
        t = parse_predictions_data(json_ptr, t, tokens, tokens[t].size, stop, time_now);
      } else {
        break;
      }
      LOG_DBG("    ]");
    } else {
      LOG_WRN(
          "Unexpected key in data: %.*s,", tokens[t].end - tokens[t].start,
          json_ptr + tokens[t].start
      );
      // We don't expect other keys, so skip their values
      t++;
    }
    // Increment 1 to get to next key
    t++;
  }
  return t;
}

int parse_swiftly_json(const char *const json_ptr, Stop *stop, unsigned int time_now) {
  jsmn_parser p;

  /** The number of maximum possible tokens we expect in our JSON string */
  jsmntok_t tokens[STOP_TOK_COUNT];

  /** The jsmn token counter. */
  int t;
  int ret;
  int err;

  jsmn_init(&p);

  /** The number of tokens *allocated* from tokens array to parse the JSON string */
  ret = jsmn_parse(&p, json_ptr, strlen(json_ptr), tokens, sizeof(tokens) / sizeof(jsmntok_t));

  err = eval_jsmn_return(ret);
  if (err) {
    LOG_ERR("Failed to parse JSON");
    return err;
  }

  LOG_INF("Tokens allocated: %d/%d", ret + 1, STOP_TOK_COUNT);

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

  LOG_DBG("{");
  /* We want to loop over all the keys of the root object.
   * We know the token after a key is a value so we skip that iteration.
   */
  while (t < ret) {
    if (jsoneq(json_ptr, &tokens[t], "success")) {
      t++;
      LOG_DBG("  success: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start);
    } else if (jsoneq(json_ptr, &tokens[t], "route")) {
      t++;
      LOG_DBG("  route: %.*s,", tokens[t].end - tokens[t].start, json_ptr + tokens[t].start);
    } else if (jsoneq(json_ptr, &tokens[t], "data")) {
      t++;
      LOG_DBG("  data: {");
      t = parse_data(json_ptr, t, tokens, tokens[t].size, stop, time_now);
      LOG_DBG("  }");
    } else {
      LOG_WRN(
          "Unexpected key in Stop: %.*s,", tokens[t].end - tokens[t].start,
          json_ptr + tokens[t].start
      );
      // We don't expect other keys, so skip their values
      t++;
    }
    // Increment 1 to get to next key
    t++;
  }
  LOG_DBG("}");
  LOG_DBG("Total tokens parsed: %d", t - 1);
  return EXIT_SUCCESS;
}
#endif  // CONFIG_STOP_REQUEST_SWIFTLY
