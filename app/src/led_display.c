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

LOG_MODULE_REGISTER(led_display, LOG_LEVEL_INF);

#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)

#define LED(_color, _brightness)                                    \
  {                                                                 \
    .r = ((gamma_lut[(uint8_t)(_color >> 16)] * _brightness) >> 8), \
    .g = ((gamma_lut[(uint8_t)(_color >> 8)] * _brightness) >> 8),  \
    .b = ((gamma_lut[(uint8_t)_color] * _brightness) >> 8)          \
  }

/** Gamma correction look up table, gamma = 2.0 */
static const uint8_t gamma_lut[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,
    1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,
    4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,
    8,   8,   9,   9,   9,   10,  10,  11,  11,  11,  12,  12,  13,  13,  14,
    14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  21,
    22,  23,  23,  24,  24,  25,  26,  26,  27,  28,  28,  29,  30,  30,  31,
    32,  32,  33,  34,  35,  35,  36,  37,  38,  38,  39,  40,  41,  42,  42,
    43,  44,  45,  46,  47,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
    56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,
    71,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  84,  85,  86,  87,
    88,  89,  91,  92,  93,  94,  95,  97,  98,  99,  100, 102, 103, 104, 105,
    107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120, 121, 123, 124, 126,
    127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143, 145, 146, 148,
    149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166, 168, 170, 171,
    173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195, 197,
    199, 200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218, 220, 222, 224,
    226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253,
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

  /** Give some time for the pin set to take effect */
  k_msleep(10);

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

#ifdef CONFIG_LED_DISPLAY_TEST
int led_test_patern(void) {
  if (!device_is_ready(strip)) {
    LOG_ERR("LED strip device %s is not ready", strip->name);
    return -1;
  }

  if (!device_is_ready(mux)) {
    LOG_ERR("MUX device %s is not ready", mux->name);
    return -1;
  }

  static const uint8_t brightness = 0x33;
  static const uint32_t color = 0xFFFFFF;
  static const struct led_rgb pixel = LED(color, brightness);

  LOG_DBG(
      "LED = {.r=%#02x}, {.g=%#02x}, {.b=%#02x}", pixel.r, pixel.g, pixel.b
  );

  LOG_INF("Running individual LED test");

  memset(&pixels[0], 0, sizeof(struct led_rgb) * STRIP_NUM_PIXELS);
  for (size_t test = 0; test < (STRIP_NUM_PIXELS / 2); test++) {
    memcpy(&pixels[test], &pixel, sizeof(struct led_rgb));
    memcpy(&pixels[63 + test], &pixel, sizeof(struct led_rgb));

    for (size_t i = 0; i < NUMBER_OF_DISPLAY_BOXES; i++) {
      mux_set_active_port(mux, i);
      if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
        LOG_ERR("Failed to update LED strip, test: %d", 0);
        return -1;
      }
    }
    k_msleep(100);
  }

  LOG_INF("Individual LED test done.");
  k_msleep(3000);

  LOG_INF("Running number display test.");
  static const DisplayBox display_boxes[] = DISPLAY_BOXES;
  for (size_t test = 0; test < 10; test++) {
    for (size_t i = 0; i < NUMBER_OF_DISPLAY_BOXES; i++) {
      if (write_num_to_display(
              &display_boxes[i], display_boxes[i].brightness, test * 111
          )) {
        return -1;
      }
    }
    k_msleep(3000);
  }

  LOG_INF("Number display test done. Setting enable pin low on all displays.");

  for (size_t i = 0; i < NUMBER_OF_DISPLAY_BOXES; i++) {
    if (display_off(i)) {
      return -1;
    }
  }
  k_msleep(3000);

  LOG_INF("Tests complete!");
  return 0;
}

int max_power_test(void) {
  static const DisplayBox display_boxes[] = DISPLAY_BOXES;

  if (!device_is_ready(strip)) {
    LOG_ERR("LED strip device %s is not ready", strip->name);
    return -1;
  }

  if (!device_is_ready(mux)) {
    LOG_ERR("MUX device %s is not ready", mux->name);
    return -1;
  }

  for (size_t box = 0; box < NUMBER_OF_DISPLAY_BOXES; box++) {
    if (display_off(box)) {
      return -1;
    }
  }

  LOG_INF("Running individual box power display test.");

  for (size_t i = 0; i < NUMBER_OF_DISPLAY_BOXES; i++) {
    if (i != 0) {
      if (display_off(i - 1)) {
        return -1;
      }
    }
    if (write_num_to_display(
            &display_boxes[i], display_boxes[i].brightness, 888
        )) {
      return -1;
    }
    k_msleep(10000);
  }

  LOG_INF("Running max power display test.");
  for (size_t j = 0; j < NUMBER_OF_DISPLAY_BOXES; j++) {
    if (write_num_to_display(
            &display_boxes[j], display_boxes[j].brightness, 888
        )) {
      return -1;
    }
  }
  k_msleep(10000);
  return 0;
}
#endif  // CONFIG_LED_DISPLAY_TEST
