/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/time.h>
#include <zephyr/types.h>

/* Newlib C includes */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* nrf lib includes */
#include <modem/lte_lc.h>

/* app includes */
#include <custom_http_client.h>
#include <jsmn_parse.h>
#include <rtc.h>
#include <stop.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define I2C1 DEVICE_DT_GET(DT_NODELABEL(i2c1))

const struct device *i2c_display = I2C1;

void i2c_display_ready() {
	/* Check device readiness */
  for (int i = 0; i < 3; i++) {
    if (!device_is_ready(i2c_display) && (i == 3 - 1)) {
		  LOG_ERR("I2C device failed to initialize after 3 attempts.");
	  } else if (!device_is_ready(i2c_display)) {
      LOG_WRN("I2C device isn't ready! Retrying...");
      k_msleep(1000);
    } else {
      break;
    }
  }
}

int write_byte_to_display(uint16_t i2c_addr, uint16_t min) {
  i2c_display_ready();
  const uint8_t i2c_tx_buffer[] = { ((min >> 8) & 0xFF), (min & 0xFF) };

  return i2c_write(i2c_display, &i2c_tx_buffer, sizeof(i2c_tx_buffer), i2c_addr);
}

// uint16_t minutes_to_departure(Departure *departure) {
//   /* EDT ex: /Date(1648627309163-0400)\, where 1648627309163 is ms since the epoch.
//   *  We don't care about the time zone and we just want the seconds.
//   *  We strip off the leading '/Date(' and trailing '-400)\' + the last 3 digits to do
//   *  a rough conversion to seconds.
//   */
//   char edt_string[11];
//   memset(edt_string, '\0', sizeof(edt_string));
//   memcpy(edt_string, ((departure->edt) + 6), 10);
//   int edt_seconds = atoi(edt_string);
//   return (uint16_t)(edt_seconds - get_rtc_time()) / 60;
// }

bool isd_unique(char *array[], char *isd, int arr_len) {
  if (isd[strlen(isd)] != '\0') {
    LOG_ERR("ISD is not zero terminated");
  }

  /* Array is empty, return true */
  // if (arr_len == 0) {
  //   return true;
  // }

  for (int i = 0; i < arr_len; i++) {
    LOG_WRN("isd: %s", isd);
    if (strcmp(array[i], isd) == 0) {
      return false;
    }
  }
  return true;
}

// static char *json_test_string = "[{\"LastUpdated\":\"/Date(1676939316553-0500)/\",\"RouteDirections\":[{\"Departures\":[],\"Direction\":\"Northbound\",\"DirectionCode\":\"N\",\"HeadwayDepartures\":null,\"IsDone\":true,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":10029,\"RouteRecordId\":2135},{\"Departures\":[],\"Direction\":\"Southbound\",\"DirectionCode\":\"S\",\"HeadwayDepartures\":null,\"IsDone\":true,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":10029,\"RouteRecordId\":2135},{\"Departures\":[],\"Direction\":\"Northbound\",\"DirectionCode\":\"N\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":20038,\"RouteRecordId\":615},{\"Departures\":[{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676940900000-0500)/\",\"EDTLocalTime\":\"2023-02-20T19:55:00\",\"ETA\":\"/Date(1676940600000-0500)/\",\"ETALocalTime\":\"2023-02-20T19:50:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939316553-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:28:36\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676940900000-0500)/\",\"SDTLocalTime\":\"2023-02-20T19:55:00\",\"STA\":\"/Date(1676940600000-0500)/\",\"STALocalTime\":\"2023-02-20T19:50:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":381,\"GtfsTripId\":\"t7A3-b17D-sl6D\",\"InternalSignDesc\":\"Mt Holyoke College via Hampshire College\",\"InternetServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"IVRServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"StopSequence\":0,\"TripDirection\":\"S\",\"TripId\":1955,\"TripRecordId\":270781,\"TripStartTime\":\"/Date(-2208899100000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T19:55:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"UMASS\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676943300000-0500)/\",\"EDTLocalTime\":\"2023-02-20T20:35:00\",\"ETA\":\"/Date(1676942700000-0500)/\",\"ETALocalTime\":\"2023-02-20T20:25:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939216097-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:26:56\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676943300000-0500)/\",\"SDTLocalTime\":\"2023-02-20T20:35:00\",\"STA\":\"/Date(1676942700000-0500)/\",\"STALocalTime\":\"2023-02-20T20:25:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":382,\"GtfsTripId\":\"t7F3-b17E-sl6D\",\"InternalSignDesc\":\"Mt Holyoke College via Hampshire College\",\"InternetServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"IVRServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"StopSequence\":0,\"TripDirection\":\"S\",\"TripId\":2035,\"TripRecordId\":270786,\"TripStartTime\":\"/Date(-2208896700000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T20:35:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"UMASS\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676945700000-0500)/\",\"EDTLocalTime\":\"2023-02-20T21:15:00\",\"ETA\":\"/Date(1676945400000-0500)/\",\"ETALocalTime\":\"2023-02-20T21:10:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939316553-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:28:36\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676945700000-0500)/\",\"SDTLocalTime\":\"2023-02-20T21:15:00\",\"STA\":\"/Date(1676945400000-0500)/\",\"STALocalTime\":\"2023-02-20T21:10:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":381,\"GtfsTripId\":\"t843-b17D-sl6D\",\"InternalSignDesc\":\"Mt Holyoke College via Hampshire College\",\"InternetServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"IVRServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"StopSequence\":0,\"TripDirection\":\"S\",\"TripId\":2115,\"TripRecordId\":270791,\"TripStartTime\":\"/Date(-2208894300000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T21:15:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"UMASS\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676948100000-0500)/\",\"EDTLocalTime\":\"2023-02-20T21:55:00\",\"ETA\":\"/Date(1676947800000-0500)/\",\"ETALocalTime\":\"2023-02-20T21:50:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939216097-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:26:56\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676948100000-0500)/\",\"SDTLocalTime\":\"2023-02-20T21:55:00\",\"STA\":\"/Date(1676947800000-0500)/\",\"STALocalTime\":\"2023-02-20T21:50:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":382,\"GtfsTripId\":\"t86B-b17E-sl6D\",\"InternalSignDesc\":\"Mt Holyoke College via Hampshire College\",\"InternetServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"IVRServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"StopSequence\":0,\"TripDirection\":\"S\",\"TripId\":2155,\"TripRecordId\":270796,\"TripStartTime\":\"/Date(-2208891900000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T21:55:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"UMASS\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676952900000-0500)/\",\"EDTLocalTime\":\"2023-02-20T23:15:00\",\"ETA\":\"/Date(1676952600000-0500)/\",\"ETALocalTime\":\"2023-02-20T23:10:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676889017043-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T05:30:17\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676952900000-0500)/\",\"SDTLocalTime\":\"2023-02-20T23:15:00\",\"STA\":\"/Date(1676952600000-0500)/\",\"STALocalTime\":\"2023-02-20T23:10:00\",\"StopFlag\":2,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":382,\"GtfsTripId\":\"t90B-b17E-sl6D\",\"InternalSignDesc\":\"Mt Holyoke College via Hampshire College\",\"InternetServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"IVRServiceDesc\":\"Mt Holyoke College via Hampshire College\",\"StopSequence\":0,\"TripDirection\":\"S\",\"TripId\":2315,\"TripRecordId\":270804,\"TripStartTime\":\"/Date(-2208887100000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T23:15:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"UMASS\"}],\"Direction\":\"Southbound\",\"DirectionCode\":\"S\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":20038,\"RouteRecordId\":615},{\"Departures\":[{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676941500000-0500)/\",\"EDTLocalTime\":\"2023-02-20T20:05:00\",\"ETA\":\"/Date(1676941500000-0500)/\",\"ETALocalTime\":\"2023-02-20T20:05:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939202877-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:26:42\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676941500000-0500)/\",\"SDTLocalTime\":\"2023-02-20T20:05:00\",\"STA\":\"/Date(1676941500000-0500)/\",\"STALocalTime\":\"2023-02-20T20:05:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4305,\"GtfsTripId\":\"t78A-b10D1-sl35\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":2100,\"TripDirection\":\"E\",\"TripId\":1930,\"TripRecordId\":66317,\"TripStartTime\":\"/Date(-2208900600000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T19:30:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676945100000-0500)/\",\"EDTLocalTime\":\"2023-02-20T21:05:00\",\"ETA\":\"/Date(1676945100000-0500)/\",\"ETALocalTime\":\"2023-02-20T21:05:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939270853-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:27:50\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676945100000-0500)/\",\"SDTLocalTime\":\"2023-02-20T21:05:00\",\"STA\":\"/Date(1676945100000-0500)/\",\"STALocalTime\":\"2023-02-20T21:05:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4306,\"GtfsTripId\":\"t7EE-b10D2-sl35\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":2100,\"TripDirection\":\"E\",\"TripId\":2030,\"TripRecordId\":66871,\"TripStartTime\":\"/Date(-2208897000000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T20:30:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676947800000-0500)/\",\"EDTLocalTime\":\"2023-02-20T21:50:00\",\"ETA\":\"/Date(1676947800000-0500)/\",\"ETALocalTime\":\"2023-02-20T21:50:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939202877-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:26:42\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676947800000-0500)/\",\"SDTLocalTime\":\"2023-02-20T21:50:00\",\"STA\":\"/Date(1676947800000-0500)/\",\"STALocalTime\":\"2023-02-20T21:50:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4305,\"GtfsTripId\":\"t843-b10D1-sl35\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":2100,\"TripDirection\":\"E\",\"TripId\":2115,\"TripRecordId\":66787,\"TripStartTime\":\"/Date(-2208894300000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T21:15:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676950200000-0500)/\",\"EDTLocalTime\":\"2023-02-20T22:30:00\",\"ETA\":\"/Date(1676950200000-0500)/\",\"ETALocalTime\":\"2023-02-20T22:30:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676883655563-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T04:00:55\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676950200000-0500)/\",\"SDTLocalTime\":\"2023-02-20T22:30:00\",\"STA\":\"/Date(1676950200000-0500)/\",\"STALocalTime\":\"2023-02-20T22:30:00\",\"StopFlag\":2,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4306,\"GtfsTripId\":\"t898-b10D2-sl35\",\"InternalSignDesc\":\"Amherst College via UMass\",\"InternetServiceDesc\":\"Amherst College via UMass\",\"IVRServiceDesc\":\"Amherst College via UMass\",\"StopSequence\":1800,\"TripDirection\":\"E\",\"TripId\":2200,\"TripRecordId\":67858,\"TripStartTime\":\"/Date(-2208891600000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T22:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"}],\"Direction\":\"East\",\"DirectionCode\":\"E\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":30043,\"RouteRecordId\":317},{\"Departures\":[{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":\"/Date(1676939269000-0500)/\",\"ATALocalTime\":\"2023-02-20T19:27:49\",\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676939700000-0500)/\",\"EDTLocalTime\":\"2023-02-20T19:35:00\",\"ETA\":\"/Date(1676939269000-0500)/\",\"ETALocalTime\":\"2023-02-20T19:27:49\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939270853-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:27:50\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676939700000-0500)/\",\"SDTLocalTime\":\"2023-02-20T19:35:00\",\"STA\":\"/Date(1676939700000-0500)/\",\"STALocalTime\":\"2023-02-20T19:35:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4306,\"GtfsTripId\":\"t780-b10D2-sl35\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":1920,\"TripRecordId\":67146,\"TripStartTime\":\"/Date(-2208901200000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T19:20:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676943300000-0500)/\",\"EDTLocalTime\":\"2023-02-20T20:35:00\",\"ETA\":\"/Date(1676943300000-0500)/\",\"ETALocalTime\":\"2023-02-20T20:35:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939202877-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:26:42\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676943300000-0500)/\",\"SDTLocalTime\":\"2023-02-20T20:35:00\",\"STA\":\"/Date(1676943300000-0500)/\",\"STALocalTime\":\"2023-02-20T20:35:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4305,\"GtfsTripId\":\"t7E4-b10D1-sl35\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":2020,\"TripRecordId\":67605,\"TripStartTime\":\"/Date(-2208897600000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T20:20:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676946900000-0500)/\",\"EDTLocalTime\":\"2023-02-20T21:35:00\",\"ETA\":\"/Date(1676946900000-0500)/\",\"ETALocalTime\":\"2023-02-20T21:35:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939270853-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:27:50\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676946900000-0500)/\",\"SDTLocalTime\":\"2023-02-20T21:35:00\",\"STA\":\"/Date(1676946900000-0500)/\",\"STALocalTime\":\"2023-02-20T21:35:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4306,\"GtfsTripId\":\"t848-b10D2-sl35\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":2120,\"TripRecordId\":66888,\"TripStartTime\":\"/Date(-2208894000000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T21:20:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676949300000-0500)/\",\"EDTLocalTime\":\"2023-02-20T22:15:00\",\"ETA\":\"/Date(1676949300000-0500)/\",\"ETALocalTime\":\"2023-02-20T22:15:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676939202877-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T19:26:42\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676949300000-0500)/\",\"SDTLocalTime\":\"2023-02-20T22:15:00\",\"STA\":\"/Date(1676949300000-0500)/\",\"STALocalTime\":\"2023-02-20T22:15:00\",\"StopFlag\":0,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4305,\"GtfsTripId\":\"t898-b10D1-sl35\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":900,\"TripDirection\":\"W\",\"TripId\":2200,\"TripRecordId\":67477,\"TripStartTime\":\"/Date(-2208891600000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T22:00:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"},{\"ADT\":null,\"ADTLocalTime\":null,\"ATA\":null,\"ATALocalTime\":null,\"Bay\":null,\"Dev\":\"00:00:00\",\"EDT\":\"/Date(1676953200000-0500)/\",\"EDTLocalTime\":\"2023-02-20T23:20:00\",\"ETA\":\"/Date(1676953200000-0500)/\",\"ETALocalTime\":\"2023-02-20T23:20:00\",\"IsCompleted\":false,\"IsLastStopOnTrip\":false,\"LastUpdated\":\"/Date(1676883655563-0500)/\",\"LastUpdatedLocalTime\":\"2023-02-20T04:00:55\",\"Mode\":0,\"ModeReportLabel\":\"Normal\",\"PropogationStatus\":0,\"SDT\":\"/Date(1676953200000-0500)/\",\"SDTLocalTime\":\"2023-02-20T23:20:00\",\"STA\":\"/Date(1676953200000-0500)/\",\"STALocalTime\":\"2023-02-20T23:20:00\",\"StopFlag\":2,\"StopStatus\":0,\"StopStatusReportLabel\":\"Scheduled\",\"Trip\":{\"BlockFareboxId\":4306,\"GtfsTripId\":\"t906-b10D2-sl35\",\"InternalSignDesc\":\"Northampton via Hampshire Mall\",\"InternetServiceDesc\":\"Northampton via Hampshire Mall\",\"IVRServiceDesc\":\"Northampton via Hampshire Mall\",\"StopSequence\":600,\"TripDirection\":\"W\",\"TripId\":2310,\"TripRecordId\":67728,\"TripStartTime\":\"/Date(-2208887400000-0500)/\",\"TripStartTimeLocalTime\":\"1900-01-01T23:10:00\",\"TripStatus\":0,\"TripStatusReportLabel\":\"Scheduled\"},\"PropertyName\":\"VATCO\"}],\"Direction\":\"West\",\"DirectionCode\":\"W\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":30043,\"RouteRecordId\":317},{\"Departures\":[],\"Direction\":\"Eastbound\",\"DirectionCode\":\"E\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":20079,\"RouteRecordId\":616},{\"Departures\":[],\"Direction\":\"Westbound\",\"DirectionCode\":\"W\",\"HeadwayDepartures\":null,\"IsDone\":false,\"IsHeadway\":false,\"IsHeadwayMonitored\":false,\"RouteId\":20079,\"RouteRecordId\":616}],\"StopId\":73,\"StopRecordId\":17223}]";

void main(void) {
  // lte_init();
  rtc_init();
  set_rtc_time();

  // TODO: Create seperate thread for time sync
  // while(true) {
  //   k_sleep(K_MINUTES(50));
  //   set_rtc_time();
  // }

  // while(true) {
  //   k_msleep(5000);
  //   LOG_INF("UTC Unix Epoc: %lld", get_rtc_time());
  // }

  uint16_t min = 0;

  if (http_request_json() != 0) {
    LOG_ERR("HTTP GET request for JSON failed; cleaning up.");
    goto cleanup;
  }

  if (parse_json_for_stop(recv_body_buf) != 0) {
    LOG_ERR("Failed to parse JSON; cleaning up.");
    goto cleanup;
  }
  // if (parse_json_for_stop(json_test_string) != 0) { goto cleanup; }

  // for (int i = 0; i < stop.routes_size; i++) {
  //   struct Route route = stop.routes[i];
  //   LOG_INF(
  //     "================ Route ID: %d, Direction Code: %c ================",
  //     route.id, route.direction
  //   );
  //   for (int j = 0; j < route.departures_size; j++) {
  //     struct Departure departure = route.departures[j];
  //     if (!departure.skipped) {
  //       min = minutes_to_departure(&departure);
  //       LOG_INF("  - %s: %d", departure.isd, min);
  //       if (route.id == 30043 && route.direction == 'W') {
  //         write_byte_to_display(B43_DISPLAY_ADDR, min);
  //       }
  //     }
  //   }
  // }
  // k_msleep(5000);

cleanup:
  lte_lc_power_off();
  LOG_WRN("Reached end of main; shutting down");
}
