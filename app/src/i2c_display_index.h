#ifndef STOP_DISPLAY_INDEX_H
#define STOP_DISPLAY_INDEX_H

/** @def B43_WEST_DISPLAY_ADDR
 *  @brief A macro that defines the I2C display address for route B43 West.
 */
#define B43_WEST_DISPLAY_ADDR 0x43

/** @def B43_EAST_DISPLAY_ADDR
 *  @brief A macro that defines the I2C display address for route B43 East.
 */
#define B43_EAST_DISPLAY_ADDR 0x45

static int get_display_address(int route_id, char direction_code) {
  switch (route_id) {
    case 10029:
      switch (direction_code) {
        case 'N':
          break;

        case 'E':
          break;

        case 'S':
          return 4;

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
          return 1;

        case 'S':
          break;

        case 'W':
          return 2;

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
          return 3;

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
