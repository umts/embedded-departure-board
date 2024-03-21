#ifndef UPDATE_STOP_H
#define UPDATE_STOP_H

#include <zephyr/kernel.h>

/** Specify the number of display boxes connected*/
#define NUMBER_OF_DISPLAY_BOXES 5

/** Specify the route, display text, and position for each display box */
// clang-format off
#define DISPLAY_BOXES {                                  \
  { .id = 30038, .position = 0, .direction_code = 'S' }, \
  { .id = 10043, .position = 1, .direction_code = 'E' }, \
  { .id = 10043, .position = 2, .direction_code = 'W' }, \
  { .id = 10943, .position = 3, .direction_code = 'W' }, \
  { .id = 20029, .position = 4, .direction_code = 'S' }  \
}
// clang-format on

typedef const struct DisplayBox {
  const char direction_code;
  const int id;
  const int position;
} DisplayBox;

void update_stop_timeout_handler(struct k_timer* timer_id);
int update_stop(void);

extern struct k_timer update_stop_timer;
extern struct k_sem stop_sem;

#endif
