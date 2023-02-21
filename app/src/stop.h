/** @file stops.h
 *  @brief De facto configuration file for the app.
*/

#ifndef STOPS_H
#define STOPS_H

  /** @def STOP_ID
   *  @brief A macro that defines the PVTA stop ID to request departures for.
   *  TODO: Allow multi-stop requests.
   *
   *  Specify what stop ID you want to query.
  */
  #define STOP_ID "73"

  /** @def ROUTE_IDS
   *  @brief A currently unused macro that defines specific routes you want departures for.
   *  TODO: Impliment selective route picking.
   *
   *  Specify what routes you want to get departure times for, leave blank for all routes.
  */
  #define ROUTE_IDS []

  /** @def MAX_ROUTES
   *  @brief A macro that defines the maximum number of routes expected for a stop.
   *
   * Specify the maximum number of routes you expect your selected stop to have.
  */
  #define MAX_ROUTES 6

  /** @def MAX_DEPARTURES
   *  @brief A macro that defines the maximum number of departures expected for a route.
   *
   *  Specify the maximum number of departures you expect each route to have.
   *  Applies to **ALL**  departures.
  */
  #define MAX_DEPARTURES 4

  /** @def B43_DISPLAY_ADDR
   *  @brief A macro that defines the I2C display address for route B43.
  */
  #define B43_DISPLAY_ADDR 0x43

  #include <zephyr/sys/timeutil.h>
  #include <zephyr/data/json.h>
  #include <inttypes.h>
  #include <stdbool.h>

  typedef struct Departure {
    int id;
    bool skipped;
    char *isd;
    int etd;
    int std;
  } Departure;

  typedef struct Route {
    char direction;
    int id;
    int departures_size;
    struct Departure departures[MAX_DEPARTURES];
  } Route;
  
  typedef struct Stop {
    int last_updated;
    const char *id;
    int routes_size;
    struct Route routes[MAX_ROUTES];
  } Stop;

  /* json_obj_descrs defined in parse_json.c */
  typedef struct JSON_TRIP {
    int block_farebox_id;
    const char *gtfs_trip_id;
    const char *internal_sign_desc;
    const char *internet_sign_desc;
    const char *ivr_sign_dec;
    int stop_sequence;
    const char *trip_direction;
    int trip_id;
    int trip_record_id;
    const char *trip_start_time;
    const char *trip_start_time_local_time;
    int trip_status;
    const char *trip_status_report_label;
  } JSON_TRIP;

  typedef struct JSON_DEPARTURE {
    bool adt;
    bool adt_local_time;
    const char *ata;
    const char *ata_local_time;
    bool bay;
    const char *dev;
    const char *edt;
    const char *edt_local_time;
    const char *eta;
    const char *eta_local_time;
    bool completed;
    bool last_stop_on_trip;
    const char *last_updated;
    const char *last_updated_local_time;
    int mode;
    const char *mode_report_label;
    int propogation_status;
    const char *sdt;
    const char *sdt_local_time;
    const char *sta;
    const char *sta_local_time;
    int stop_flag;
    int stop_status;
    const char *stop_status_report_label;
    JSON_TRIP trip;
    const char *property_name;
  } JSON_DEPARTURE;

  typedef struct JSON_ROUTE_DIRECTION {
    JSON_DEPARTURE departures[MAX_DEPARTURES];
    const char *direction;
    const char *direction_code;
    bool headway_departures;
    bool done;
    bool headway;
    bool headway_monitored;
    int route_id;
    int route_record_id;
    size_t departures_size;
  } JSON_ROUTE_DIRECTION;

  typedef struct JSON_STOP {
    const char *last_updated;
    JSON_ROUTE_DIRECTION directions[MAX_ROUTES];
    int id;
    int record_id;
    size_t directions_size;
  } JSON_STOP;
#endif
