#include <led_display.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
// #include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "zephyr/devicetree.h"

LOG_MODULE_REGISTER(led_display, LOG_LEVEL_DBG);

#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)
#define ROW_LENGTH 1
#define NUM_ROWS (STRIP_NUM_PIXELS / ROW_LENGTH)

#define LED(_color, _brightness)                                    \
  {                                                                 \
    .r = ((uint8_t)(((uint8_t)(_color >> 16) * _brightness) >> 8)), \
    .g = ((uint8_t)(((uint8_t)(_color >> 8) * _brightness) >> 8)),  \
    .b = ((uint8_t)(((uint8_t)_color * _brightness) >> 8))          \
  }

static const uint32_t colors[] = {
    0x7E0A6D, 0x00467E, 0x00467E, 0x00467E, 0xFF0000};

/** 4 x 7 digit binary pixel map */
// static const uint32_t digit_pixel_map_x[] = {
//     0xF99999F0, 0x26222270, 0xF11F88F0, 0xF11F11F0, 0x999F1110,
//     0xF88F11F0, 0xF88F99F0, 0xF1111110, 0xF99F99F0, 0xF99F11F0};

/** 7 segment binary pixel map */
static const uint8_t digit_segment_map[] = {0x7E, 0x30, 0x6D, 0x79, 0x33,
                                            0x5B, 0x5F, 0x70, 0x7F, 0x7B};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));

static int display_digit(
    size_t display, uint8_t brightness, size_t offset, size_t digit
) {
  int rc;
  size_t seg_offset = 0;
  size_t pos = display * 126;

  struct led_rgb color = LED(colors[display], brightness);

  for (size_t segment = 0; segment < 7; segment++) {
    if (0b01000000 & (digit_segment_map[digit] << segment)) {
      /* Panel 1 */
      // memcpy(
      //     &pixels[0 + seg_offset + offset + pos], &color,
      //     (sizeof(struct led_rgb) * 3)
      // );
      memcpy(
          &pixels[0 + seg_offset + offset + pos], &color, sizeof(struct led_rgb)
      );
      memcpy(
          &pixels[2 + seg_offset + offset + pos], &color, sizeof(struct led_rgb)
      );
      memcpy(
          &pixels[3 + seg_offset + offset + pos], &color, sizeof(struct led_rgb)
      );

      /* Panel 2 */
      // memcpy(
      //     &pixels[63 + seg_offset + offset + pos], &color,
      //     (sizeof(struct led_rgb) * 3)
      // );
      memcpy(
          &pixels[63 + seg_offset + offset + pos], &color,
          sizeof(struct led_rgb)
      );
      memcpy(
          &pixels[64 + seg_offset + offset + pos], &color,
          sizeof(struct led_rgb)
      );
      memcpy(
          &pixels[65 + seg_offset + offset + pos], &color,
          sizeof(struct led_rgb)
      );
    }
    seg_offset += 3;
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

  // Turn off all pixels for the given display
  memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);

  if (num > 999) {
    led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
  } else if (num > 99) {
    display_digit(display, brightness, 0, num % 10);
    display_digit(display, brightness, 21, num / 10 % 10);
    display_digit(display, brightness, 42, num / 100 % 10);
  } else if (num > 9) {
    display_digit(display, brightness, 0, num % 10);
    display_digit(display, brightness, 21, num / 10 % 100);
  } else {
    display_digit(display, brightness, 0, num);
  }

  return 0;
}

int turn_display_off(size_t display) {
  memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);
  if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
    LOG_ERR("Failed to update LED strip, test: %d", display);
  }
  return 0;
}

#ifdef CONFIG_DEBUG
void led_test_patern() {
  // static const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff};
  uint8_t brightness = 0x55;
  uint32_t color = 0x7E0A6D;
  struct led_rgb pixel = LED(color, brightness);

  // LOG_DBG(
  //     "LED = {.r=%#02x}, {.g=%#02x}, {.b=%#02x}", pixel.r, pixel.g, pixel.b
  // );

  // turn_display_off(0);
  // memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);
  // k_msleep(100);
  // for (size_t test = 0; test < (STRIP_NUM_PIXELS / 2); test++) {
  // memcpy(&pixels[0], &pixel, sizeof(struct led_rgb));
  // memcpy(&pixels[63], &pixel, sizeof(struct led_rgb));
  k_msleep(1000);
  if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
    LOG_ERR("Failed to update LED strip, test: %d", 0);
  }
  write_num_to_display(0, brightness, 222);
  //   k_msleep(1100);
  // }

  // for (size_t test = 0; test < 4; test++) {
  //   struct led_rgb pixel = LED((uint32_t)colors[test],
  //   (uint8_t)brightness);
  //   // struct led_rgb pixel = {
  //   //     .r = (((uint8_t)(colors[test] >> 16) * 0x55) >> 8),
  //   //     .g = (((uint8_t)(colors[test] >> 8) * 0x55) >> 8),
  //   //     .b = (((uint8_t)colors[test] * 0x55) >> 8)};
  //   memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);
  //   k_msleep(10);
  //   for (size_t cursor = 0; cursor < STRIP_NUM_PIXELS; cursor++) {
  //     memcpy(&pixels[cursor], &pixel, sizeof(struct led_rgb));
  //   }
  //   k_msleep(100);
  //   if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
  //     LOG_ERR("Failed to update LED strip, test: %d", test);
  //   }
  //   k_msleep(990);
  // }
}
#endif
