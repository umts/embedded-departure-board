#ifndef I2C_DISPLAY_INDEX_H
#define I2C_DISPLAY_INDEX_H

static int get_display_address(int route_id, char direction_code) {
  switch (route_id) {
    case 10029:
      switch (direction_code) {
        case 'N':
          break;

        case 'E':
          break;

        case 'S':
          return 8;

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
