#include <led_display.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_display, LOG_LEVEL_DBG);

#define STRIP_NUM_PIXELS DT_PROP(DT_NODELABEL(led_strip), chain_length)
#define ROW_LENGTH 16
#define NUM_ROWS (STRIP_NUM_PIXELS / ROW_LENGTH)

#define RGB(_r, _g, _b) \
  { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
    RGB(0x7E, 0x0A, 0x6D), RGB(0x00, 0x46, 0x7e), RGB(0xff, 0x00, 0x00)};

// static const uint8_t digit_pixel_map_y[10][4] = {
//     {0x3f, 0x51, 0x49, 0x3f}, {0x0, 0x22, 0x3f, 0x20},
//     {0x79, 0x49, 0x49, 0x4f}, {0x49, 0x49, 0x49, 0x3f},
//     {0x0f, 0x10, 0x10, 0x3f}, {0x4f, 0x49, 0x49, 0x79},
//     {0x3f, 0x49, 0x49, 0x79}, {0x01, 0x01, 0x01, 0x3f},
//     {0x3f, 0x49, 0x49, 0x3f}, {0x4f, 0x49, 0x49, 0x3f}};

/** 4 x 7 digit map */
static const unsigned int digit_pixel_map_x[] = {
    0xf99999f0, 0x26222270, 0xf11f88f0, 0xf11f11f0, 0x999f1110,
    0xf88f11f0, 0xf88f99f0, 0xf1111110, 0xf99f99f0, 0xf99f11f0};

struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip =
    DEVICE_DT_GET(DT_NODELABEL(led_strip));

static int display_digit(int digit, size_t offset) {
  size_t color = 1;
  int rc;

  unsigned int digit_map = digit_pixel_map_x[digit];

  if (!device_is_ready(strip)) {
    LOG_ERR("LED strip device %s is not ready", strip->name);
    return 1;
  }

  for (size_t cursor = 0; cursor < NUM_ROWS; cursor++) {
    for (size_t column = 0; column < 4; column++) {
      if (digit_map & (1UL << (31 - (cursor * 4) - column))) {
        memcpy(
            &pixels[(cursor * ROW_LENGTH) + offset + column], &colors[color],
            sizeof(struct led_rgb)
        );
      }
    }
  }

  rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

  if (rc) {
    LOG_ERR("couldn't update strip: %d", rc);
    return 1;
  }
  return 0;
}

int write_num_to_display(int num) {
  // Turn off all pixels
  memset(&pixels, 0x00, sizeof(pixels));

  if (num > 999) {
    led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
  } else if (num > 99) {
    display_digit(num % 10, 11);
    display_digit(num / 10 % 10, 6);
    display_digit(num / 100 % 10, 1);
  } else if (num > 9) {
    display_digit(num % 10, 8);
    display_digit(num / 10 % 100, 3);
  } else {
    display_digit(num, 6);
  }

  return 0;
}
