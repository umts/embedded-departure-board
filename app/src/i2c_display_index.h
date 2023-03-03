#ifndef I2C_DISPLAY_INDEX_H
#define I2C_DISPLAY_INDEX_H

/** @def B43_WEST_DISPLAY_ADDR
 *  @brief A macro that defines the I2C display address for route B43 West.
 */
#define B43_WEST_DISPLAY_ADDR 0x43

/** @def B43_EAST_DISPLAY_ADDR
 *  @brief A macro that defines the I2C display address for route B43 East.
 */
#define B43_EAST_DISPLAY_ADDR 0x45

static unsigned short get_display_address(int route_id, char direction_code) {
  switch (route_id) {
    case 10029:
      switch (direction_code) {
        case 'N':
          break;

        case 'E':
          break;

        case 'S':
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
          break;

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
          return B43_EAST_DISPLAY_ADDR;

        case 'S':
          break;

        case 'W':
          return B43_WEST_DISPLAY_ADDR;

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
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }
  return 0;
}

#endif
