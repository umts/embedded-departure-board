#ifndef UPDATE_STOP_H
#define UPDATE_STOP_H

#include <zephyr/kernel.h>

/** Specify the number of display boxes connected*/
#define NUMBER_OF_DISPLAY_BOXES 5

/** Specify the route, display text, and position for each display box */
// clang-format off
#define DISPLAY_BOXES {                                                                         \
  { .id = 30038, .position = 0, .direction_code = 'S', .color = 0x7E0A6D, .brightness = 0x69 }, \
  { .id = 10043, .position = 1, .direction_code = 'E', .color = 0x00467E, .brightness = 0x80 }, \
  { .id = 10043, .position = 2, .direction_code = 'W', .color = 0x00467E, .brightness = 0x80 }, \
  { .id = 10943, .position = 3, .direction_code = 'W', .color = 0x00467E, .brightness = 0x80 }, \
  { .id = 20029, .position = 4, .direction_code = 'S', .color = 0xFF0000, .brightness = 0x28 }  \
}
// clang-format on

/** @param brightness The max brightness allowed with all LEDS (888) on  without
 * going above the 126mA per display limit */
typedef const struct DisplayBox {
  const char direction_code;
  const int id;
  const int position;
  const uint32_t color;
  const uint8_t brightness;
} DisplayBox;

void update_stop_timeout_handler(struct k_timer* timer_id);
int update_stop(void);

extern struct k_timer update_stop_timer;
extern struct k_sem stop_sem;

#endif
