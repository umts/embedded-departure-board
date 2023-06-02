#ifndef I2C_DISPLAY_INDEX_H
#define I2C_DISPLAY_INDEX_H

#include <string.h>

static int get_display_address(const int route_id, const char direction_code,
                               const char *display_text) {
  switch (route_id) {
    case 10029:
      switch (direction_code) {
        case 'N':
          break;

        case 'E':
          break;

        case 'S':
          if (!strcmp("Holyoke Transportation Ctr via Route 116", display_text)) {
            return 8;
          }
          break;

        case 'W':
          break;

        default:
          break;
      }
      break;

    case 20038:
      switch (direction_code) {
        case 'N':
          break;

        case 'E':
          break;

        case 'S':
          return 0;

        case 'W':
          break;

        default:
          break;
      }
      break;

    case 30043:
      switch (direction_code) {
        case 'N':
          break;

        case 'E':
          return 2;

        case 'S':
          break;

        case 'W':
          return 4;

        default:
          break;
      }
      break;

    case 30943:
      switch (direction_code) {
        case 'N':
          break;

        case 'E':
          break;

        case 'S':
          break;

        case 'W':
          return 6;

        default:
          break;
      }
      break;

    default:
      break;
  }
  return -1;
}

#endif
