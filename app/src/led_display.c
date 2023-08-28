#include <led_display.h>
#include <sys/_stdint.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_display, LOG_LEVEL_DBG);

#define STRIP_NUM_PIXELS DT_PROP(DT_NODELABEL(led_strip), chain_length)
#define PIXELS_PER_DISPLAY 128
#define ROW_LENGTH 16
#define NUM_ROWS (PIXELS_PER_DISPLAY / ROW_LENGTH)

#define LED(_color, _brightness)                                    \
  {                                                                 \
    .r = ((uint8_t)(((uint8_t)(_color >> 16) * _brightness) >> 8)), \
    .g = ((uint8_t)(((uint8_t)(_color >> 8) * _brightness) >> 8)),  \
    .b = ((uint8_t)(((uint8_t)_color * _brightness) >> 8))          \
  }

static const uint32_t colors[] = {
    0x7E0A6D, 0x00467E, 0x00467E, 0x00467E, 0xFF0000};

/** 4 x 7 digit binary pixel map */
static const uint32_t digit_pixel_map_x[] = {
    0xF99999F0, 0x26222270, 0xF11F88F0, 0xF11F11F0, 0x999F1110,
    0xF88F11F0, 0xF88F99F0, 0xF1111110, 0xF99F99F0, 0xF99F11F0};

struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip =
    DEVICE_DT_GET(DT_NODELABEL(led_strip));

static int display_digit(
    size_t display, uint8_t brightness, size_t offset, size_t digit
) {
  int rc;

  struct led_rgb color = LED(colors[display], brightness);

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

  // for (size_t side = 0; side < 2; side++) {
  //   offset += (side * PIXELS_PER_DISPLAY);

  if (num > 999) {
    led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
  } else if (num > 99) {
    display_digit(display, brightness, offset + 11, num % 10);
    display_digit(display, brightness, offset + 6, num / 10 % 10);
    display_digit(display, brightness, offset + 1, num / 100 % 10);
  } else if (num > 9) {
    display_digit(display, brightness, offset + 8, num % 10);
    display_digit(display, brightness, offset + 3, num / 10 % 100);
  } else {
    display_digit(display, brightness, offset + 6, num);
  }
  // }

  return 0;
}

int turn_display_off(size_t display) {
  memset(
      &pixels[display * PIXELS_PER_DISPLAY], 0,
      sizeof(struct led_rgb) * PIXELS_PER_DISPLAY
  );
  if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
    LOG_ERR("Failed to update LED strip, test: %d", display);
  }
  return 0;
}

#ifdef CONFIG_DEBUG
void led_test_patern() {
  static const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff};
  uint8_t brightness = 0x55;

  for (size_t test = 0; test < 4; test++) {
    struct led_rgb pixel = LED((uint32_t)colors[test], (uint8_t)brightness);
    // struct led_rgb pixel = {
    //     .r = (((uint8_t)(colors[test] >> 16) * 0x55) >> 8),
    //     .g = (((uint8_t)(colors[test] >> 8) * 0x55) >> 8),
    //     .b = (((uint8_t)colors[test] * 0x55) >> 8)};
    memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);
    k_msleep(10);
    for (size_t cursor = 0; cursor < STRIP_NUM_PIXELS; cursor++) {
      memcpy(&pixels[cursor], &pixel, sizeof(struct led_rgb));
    }
    k_msleep(1000);
    if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
      LOG_ERR("Failed to update LED strip, test: %d", test);
    }
  }
}
#endif
