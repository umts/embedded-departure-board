#include <led_display.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/led/is31fl3733.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_display, LOG_LEVEL_DBG);

#define ROWS 12
#define COLS 16

/** @def  LED(x, y) ((x) * COLS) + (y)
 *  Calulates the hex addess of an LED in the grid.
 */
#define LED(x, y) ((x) * COLS) + (y)

// const struct device *led_panel0 = DEVICE_DT_GET(DT_NODELABEL(led_display0));
// const struct device *led_panel1 = DEVICE_DT_GET(DT_NODELABEL(led_display1));

static const struct device *led_panels[] = {
    DEVICE_DT_GET(DT_NODELABEL(led_display0)),
    DEVICE_DT_GET(DT_NODELABEL(led_display1))
};

typedef struct RGB_LED {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} RGB_LED;

static uint8_t leds[(ROWS * COLS)];

/** @var static const uint8_t $seven_segment_truth_table[10]
  An array of uint8_t numbers whose bits represent which segments (G through A)
  are required for a digit. i.e. uint8_t seg = [unused, seg G, ..., seg A]
 */
static const uint8_t seven_segment_truth_table[10] = {
    0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
    0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111
};

static const uint8_t inverted_y_seven_segment_truth_table[10] = {
    0b00111111, 0b00110000, 0b01011011, 0b01111001, 0b01110100,
    0b01101101, 0b01101111, 0b00111000, 0b01111111, 0b01111101
};

/** Gamma correction look up table, gamma = 1.8
 */
const uint8_t gamma_lut[256] = {
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

static const RGB_LED digits[3][7][3] = {
    {{{LED(7, 4), LED(6, 4), LED(8, 4)},
      {LED(4, 4), LED(3, 4), LED(5, 4)},
      {LED(1, 4), LED(0, 4), LED(2, 4)}},

     {{LED(7, 1), LED(6, 1), LED(8, 1)},
      {LED(4, 1), LED(3, 1), LED(5, 1)},
      {LED(1, 1), LED(0, 1), LED(2, 1)}},

     {{LED(7, 0), LED(6, 0), LED(8, 0)},
      {LED(4, 0), LED(3, 0), LED(5, 0)},
      {LED(1, 0), LED(0, 0), LED(2, 0)}},

     {{LED(7, 2), LED(6, 2), LED(8, 2)},
      {LED(4, 2), LED(3, 2), LED(5, 2)},
      {LED(1, 2), LED(0, 2), LED(2, 2)}},

     {{LED(1, 5), LED(0, 5), LED(2, 5)},
      {LED(4, 5), LED(3, 5), LED(5, 5)},
      {LED(7, 5), LED(6, 5), LED(8, 5)}},

     {{LED(7, 6), LED(6, 6), LED(8, 6)},
      {LED(4, 6), LED(3, 6), LED(5, 6)},
      {LED(1, 6), LED(0, 6), LED(2, 6)}},

     {{LED(7, 3), LED(6, 3), LED(8, 3)},
      {LED(4, 3), LED(3, 3), LED(5, 3)},
      {LED(1, 3), LED(0, 3), LED(2, 3)}}},
    {{{LED(1, 11), LED(0, 11), LED(2, 11)},
      {LED(4, 11), LED(3, 11), LED(5, 11)},
      {LED(7, 11), LED(6, 11), LED(8, 11)}},

     {{LED(1, 8), LED(0, 8), LED(2, 8)},
      {LED(4, 8), LED(3, 8), LED(5, 8)},
      {LED(7, 8), LED(6, 8), LED(8, 8)}},

     {{LED(1, 7), LED(0, 7), LED(2, 7)},
      {LED(4, 7), LED(3, 7), LED(5, 7)},
      {LED(7, 7), LED(6, 7), LED(8, 7)}},

     {{LED(1, 9), LED(0, 9), LED(2, 9)},
      {LED(4, 9), LED(3, 9), LED(5, 9)},
      {LED(7, 9), LED(6, 9), LED(8, 9)}},

     {{LED(1, 12), LED(0, 12), LED(2, 12)},
      {LED(4, 12), LED(3, 12), LED(5, 12)},
      {LED(7, 12), LED(6, 12), LED(8, 12)}},

     {{LED(1, 13), LED(0, 13), LED(2, 13)},
      {LED(4, 13), LED(3, 13), LED(5, 13)},
      {LED(7, 13), LED(6, 13), LED(8, 13)}},

     {{LED(1, 10), LED(0, 10), LED(2, 10)},
      {LED(4, 10), LED(3, 10), LED(5, 10)},
      {LED(7, 10), LED(6, 10), LED(8, 10)}}},
    {{{LED(7, 15), LED(6, 15), LED(8, 15)},
      {LED(10, 14), LED(9, 14), LED(11, 14)},
      {LED(7, 14), LED(6, 14), LED(8, 14)}},

     {{LED(4, 14), LED(3, 14), LED(5, 14)},
      {LED(1, 14), LED(0, 14), LED(2, 14)},
      {LED(10, 8), LED(9, 8), LED(11, 8)}},

     {{LED(10, 7), LED(9, 7), LED(11, 7)},
      {LED(10, 1), LED(9, 1), LED(11, 1)},
      {LED(10, 0), LED(9, 0), LED(11, 0)}},

     {{LED(10, 4), LED(9, 4), LED(11, 4)},
      {LED(10, 3), LED(9, 3), LED(11, 3)},
      {LED(10, 2), LED(9, 2), LED(11, 2)}},

     {{LED(10, 12), LED(9, 12), LED(11, 12)},
      {LED(10, 6), LED(9, 6), LED(11, 6)},
      {LED(10, 5), LED(9, 5), LED(11, 5)}},

     {{LED(4, 15), LED(3, 15), LED(5, 15)},
      {LED(1, 15), LED(0, 15), LED(2, 15)},
      {LED(10, 13), LED(9, 13), LED(11, 13)}},

     {{LED(10, 11), LED(9, 11), LED(11, 11)},
      {LED(10, 10), LED(9, 10), LED(11, 10)},
      {LED(10, 9), LED(9, 9), LED(11, 9)}}}
};

// int turn_display_off(size_t display) {
//   memset(
//       &pixels[display * PIXELS_PER_DISPLAY], 0,
//       sizeof(struct led_rgb) * PIXELS_PER_DISPLAY
//   );
//   if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
//     LOG_ERR("Failed to update LED strip, test: %d", display);
//   }
//   return 0;
// }

static int display_digit(
    const struct device *led, unsigned int digit, unsigned int number,
    const uint32_t *color, int invert_y
) {
  // uint8_t color;
  uint8_t segment_bin;
  if (invert_y) {
    segment_bin = inverted_y_seven_segment_truth_table[number];
  } else {
    segment_bin = seven_segment_truth_table[number];
  }

  for (unsigned int s = 0; s < 8; s++) {
    if ((segment_bin >> s) & (uint8_t)0b00000001) {
      // LOG_DBG("\nSegment: %d", s);
      for (unsigned int p = 0; p < 3; p++) {
        // LOG_DBG(
        //     "digits[%d][%d][%d].r = 0x%08x", digit, s, p,
        //     digits[digit][s][p].r
        // );
        leds[digits[digit][s][p].r] = gamma_lut[(uint8_t)(*color >> 16)];
        // leds[digits[digit][s][p].r] = color;

        // LOG_DBG(
        //     "digits[%d][%d][%d].g = 0x%08x", digit, s, p,
        //     digits[digit][s][p].g
        // );
        leds[digits[digit][s][p].g] = gamma_lut[(uint8_t)(*color >> 8)];
        // leds[digits[digit][s][p].g] = color;

        // LOG_DBG(
        //     "digits[%d][%d][%d].b = 0x%08x", digit, s, p,
        //     digits[digit][s][p].b
        // );
        leds[digits[digit][s][p].b] = gamma_lut[(uint8_t)*color];
        // leds[digits[digit][s][p].b] = color;
      }
    }
  }

  return 0;
}

int write_num_to_display(
    unsigned int display, unsigned int num, const uint32_t *color, int invert_y
) {
  int ret;
  uint8_t current_limit;

  /** Sets the max current based on color to ensure each panel is within the
   * power budget and matches brightness with other colors
   */
  switch (*color) {
    case 0x7E0A6D:
      LOG_DBG("Current limit set to 255");
      current_limit = 205;
      break;
    case 0x00467E:
      LOG_DBG("Current limit set to 255");
      current_limit = 255;
      break;
    default:
      LOG_DBG("Current limit set to 102");
      current_limit = 102;
      break;
  }

  if (!device_is_ready(led_panels[display])) {
    for (int i = 0; i < 3; i++) {
      if (!device_is_ready(led_panels[display]) && (i == 3 - 1)) {
        LOG_ERR("pcf85063a failed to initialize after 3 attempts.");
      } else if (!device_is_ready(led_panels[display])) {
        LOG_WRN("LED device is not ready. Retrying...");
        k_msleep(1000);
      } else {
        break;
      }
    }
    return 1;
  }

  ret = is31fl3733_current_limit(led_panels[display], current_limit);
  if (ret) {
    LOG_ERR("Could not set LED current limit (%d)\n", ret);
    goto fail;
  }

  // Turn off all pixels for the given display
  memset(leds, 0, sizeof(leds));

  if (num > 999) {
    LOG_ERR("Number is too large to fit!");
    return 1;
  } else if (num > 99) {
    display_digit(led_panels[display], 2, num % 10, color, invert_y);
    display_digit(led_panels[display], 1, num / 10 % 10, color, invert_y);
    display_digit(led_panels[display], 0, num / 100 % 10, color, invert_y);
  } else if (num > 9) {
    display_digit(led_panels[display], 1, num % 10, color, invert_y);
    display_digit(led_panels[display], 0, num / 10 % 100, color, invert_y);
  } else {
    display_digit(led_panels[display], 0, num, color, invert_y);
  }

  ret = led_write_channels(led_panels[display], 0, sizeof(leds), leds);

fail:
  return ret;
}

#ifdef CONFIG_DEBUG
static const uint32_t colors[] = {0x7E0A6D, 0x00467E, 0xFF0000};

void led_test_pattern(void) {
  for (int i = 0; i < 3; i++) {
    write_num_to_display(0, 888, &colors[i], 1);
    // write_num_to_display(1, 888, &colors[i], 0);
    k_sleep(K_SECONDS(2));
  }
}
#endif
