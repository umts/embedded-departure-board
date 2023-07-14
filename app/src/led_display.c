#include <led_display.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_display, LOG_LEVEL_DBG);

#define STRIP_NUM_PIXELS DT_PROP(DT_NODELABEL(led_strip), chain_length)
#define PIXELS_PER_DISPLAY 128
#define ROW_LENGTH 16
#define NUM_ROWS (PIXELS_PER_DISPLAY / ROW_LENGTH)

static const uint32_t colors[] = {
    0x7E0A6D, 0x00467e, 0x00467e, 0x00467e, 0xff0000};

/** 4 x 7 digit binary pixel map */
static const uint32_t digit_pixel_map_x[] = {
    0xf99999f0, 0x26222270, 0xf11f88f0, 0xf11f11f0, 0x999f1110, 0xf88f11f0,
    0xf88f99f0, 0xf1111110, 0xf99f99f0, 0xf99f11f0, 0x12484210};

struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip =
    DEVICE_DT_GET(DT_NODELABEL(led_strip));

static int display_digit(
    size_t display, uint8_t brightness, size_t offset, size_t digit
) {
  int rc;

  struct led_rgb color = {
      .r = (((uint8_t)(colors[display] >> 16) * brightness) >> 8),
      .g = (((uint8_t)(colors[display] >> 8) * brightness) >> 8),
      .b = (((uint8_t)colors[display] * brightness) >> 8)};

  uint32_t digit_map = digit_pixel_map_x[digit];

  for (size_t cursor = 0; cursor < NUM_ROWS; cursor++) {
    for (size_t column = 0; column < 4; column++) {
      if (digit_map & (1UL << (31 - (cursor * 4) - column))) {
        memcpy(
            &pixels[(cursor * ROW_LENGTH) + offset + column], &color,
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

int write_num_to_display(size_t display, uint8_t brightness, unsigned int num) {
  if (!device_is_ready(strip)) {
    LOG_ERR("LED strip device %s is not ready", strip->name);
    return 1;
  }

  size_t offset = display * PIXELS_PER_DISPLAY;

  // Turn off all pixels for the given display
  memset(&pixels[offset], 0, sizeof(struct led_rgb) * PIXELS_PER_DISPLAY);

  if (num > 999) {
    led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
  } else if (num > 99) {
    display_digit(display, brightness, offset + 11, num % 10);
    display_digit(display, brightness, offset + 6, num / 10 % 10);
    display_digit(display, brightness, offset + 1, num / 100 % 10);
  } else if (num > 9) {
    display_digit(display, brightness, offset + 8, num % 10);
    display_digit(display, brightness, offset + 3, num / 10 % 100);
  } else if (num == 0) {
    display_digit(display, brightness, offset + 8, 1);
    display_digit(display, brightness, offset + 3, 10);
  } else {
    display_digit(display, brightness, offset + 6, num);
  }

  return 0;
}
