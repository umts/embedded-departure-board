#include "led_display.h"

#include <drivers/multiplexer/multiplexer.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "display_switches.h"

#if !DT_NODE_EXISTS(DT_ALIAS(mux))
#error "Multiplexer device node with alias 'mux' not defined."
#endif

#if !DT_NODE_EXISTS(DT_ALIAS(led_strip))
#error "LED strip device node with alias 'led_strip' not defined."
#endif

LOG_MODULE_REGISTER(led_display, LOG_LEVEL_DBG);

#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)

#define LED(_color, _brightness)                                    \
  {                                                                 \
    .r = ((gamma_lut[(uint8_t)(_color >> 16)] * _brightness) >> 8), \
    .g = ((gamma_lut[(uint8_t)(_color >> 8)] * _brightness) >> 8),  \
    .b = ((gamma_lut[(uint8_t)_color] * _brightness) >> 8)          \
  }

/** Gamma correction look up table, gamma = 1.8 */
static const uint8_t gamma_lut[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,
    2,   2,   2,   2,   2,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,
    5,   6,   6,   6,   7,   7,   8,   8,   8,   9,   9,   10,  10,  10,  11,
    11,  12,  12,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,
    19,  19,  20,  21,  21,  22,  22,  23,  24,  24,  25,  26,  26,  27,  28,
    28,  29,  30,  30,  31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  38,
    39,  40,  41,  41,  42,  43,  44,  45,  46,  46,  47,  48,  49,  50,  51,
    52,  53,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,
    66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
    81,  82,  83,  84,  86,  87,  88,  89,  90,  91,  92,  93,  95,  96,  97,
    98,  99,  100, 102, 103, 104, 105, 107, 108, 109, 110, 111, 113, 114, 115,
    116, 118, 119, 120, 122, 123, 124, 126, 127, 128, 129, 131, 132, 134, 135,
    136, 138, 139, 140, 142, 143, 145, 146, 147, 149, 150, 152, 153, 154, 156,
    157, 159, 160, 162, 163, 165, 166, 168, 169, 171, 172, 174, 175, 177, 178,
    180, 181, 183, 184, 186, 188, 189, 191, 192, 194, 195, 197, 199, 200, 202,
    204, 205, 207, 208, 210, 212, 213, 215, 217, 218, 220, 222, 224, 225, 227,
    229, 230, 232, 234, 236, 237, 239, 241, 243, 244, 246, 248, 250, 251, 253,
    255
};

/** 7 segment binary pixel map */
static const uint8_t digit_segment_map[] = {0x7E, 0x30, 0x6D, 0x79, 0x33,
                                            0x5B, 0x5F, 0x70, 0x7F, 0x7B};

static struct led_rgb pixels[STRIP_NUM_PIXELS];
static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));
static const struct device *const mux = DEVICE_DT_GET(DT_ALIAS(mux));

static int display_digit(
    DisplayBox *display, uint8_t brightness, size_t offset, size_t digit
) {
  int rc;
  size_t seg_offset = 0;
  struct led_rgb color = LED(display->color, brightness);

  for (size_t segment = 0; segment < 7; segment++) {
    if (0b01000000 & (digit_segment_map[digit] << segment)) {
      /* Panel 1 */
      memcpy(&pixels[0 + seg_offset + offset], &color, sizeof(struct led_rgb));
      memcpy(&pixels[1 + seg_offset + offset], &color, sizeof(struct led_rgb));
      memcpy(&pixels[2 + seg_offset + offset], &color, sizeof(struct led_rgb));

      /* Panel 2 */
      memcpy(&pixels[63 + seg_offset + offset], &color, sizeof(struct led_rgb));
      memcpy(&pixels[64 + seg_offset + offset], &color, sizeof(struct led_rgb));
      memcpy(&pixels[65 + seg_offset + offset], &color, sizeof(struct led_rgb));
    }
    seg_offset += 3;
  }

  rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
  if (rc) {
    LOG_ERR("Couldn't update LED strip: %d", rc);
    return 1;
  }

  return 0;
}

int write_num_to_display(
    DisplayBox *display, uint8_t brightness, unsigned int num
) {
  if (display_on(display->position)) {
    LOG_ERR("Failed to enable display");
    return -1;
  }

  // Turn off all pixels for the given display
  memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);

  if (mux_set_active_port(mux, display->position)) {
    LOG_ERR("Failed to set correct mux channel");
    return -1;
  }

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

int turn_display_off(size_t display_position) {
  mux_set_active_port(mux, display_position);
  memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);
  if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
    LOG_ERR("Failed to update LED strip, test: %d", display_position);
  }
  return 0;
}

#ifdef CONFIG_DEBUG
void led_test_patern(void) {
  if (!device_is_ready(strip)) {
    LOG_ERR("LED strip device %s is not ready", strip->name);
  }

  if (!device_is_ready(mux)) {
    LOG_ERR("MUX device %s is not ready", mux->name);
  }

  uint8_t brightness = 0x33;
  uint32_t color = 0xFFFFFF;
  struct led_rgb pixel = LED(color, brightness);

  memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);
  for (size_t test = 0; test < (STRIP_NUM_PIXELS / 2); test++) {
    memcpy(&pixels[test], &pixel, sizeof(struct led_rgb));
    memcpy(&pixels[63 + test], &pixel, sizeof(struct led_rgb));

    for (size_t i = 0; i < NUMBER_OF_DISPLAY_BOXES; i++) {
      mux_set_active_port(mux, i);
      if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
        LOG_ERR("Failed to update LED strip, test: %d", 0);
        return;
      }
    }
    k_msleep(200);
  }

  static const DisplayBox display_boxes[] = DISPLAY_BOXES;
  for (size_t test = 0; test < 10; test++) {
    for (size_t i = 0; i < NUMBER_OF_DISPLAY_BOXES; i++) {
      write_num_to_display(
          &display_boxes[i], display_boxes[i].brightness, test * 111
      );
    }
    k_msleep(3000);
  }

  for (size_t i = 0; i < NUMBER_OF_DISPLAY_BOXES; i++) {
    display_off(i);
  }
  k_msleep(3000);
}
#endif
