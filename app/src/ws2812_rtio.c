#include <stdlib.h>

#include "zephyr/drivers/led_strip.h"
#define DT_DRV_COMPAT worldsemi_ws2812_rtio

// #ifdef CONFIG_NRFX_TIMER
// #include <nrfx_timer.h>

// static const nrfx_timer_t m_timer_count = NRFX_TIMER_INSTANCE(1);
// #endif

#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/led/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ws2812_rtio, LOG_LEVEL_DBG);

#define BIT_LEN 24
#define PULSE_WIDTH_NS 250

struct ws2812_rtio_config {
  struct gpio_dt_spec gpios;
  uint8_t num_colors;
  const uint8_t *color_mapping;
  size_t chain_length;
};

struct ws2812_rtio_data {
  struct rtio_iodev iodev;
  struct k_timer timer;
  const struct device *dev;
};

// static void ws2812_rtio_update_rgb(const struct device *dev) {
//   LOG_DBG("Entering ws2812_rtio_update_rgb");
//   const struct ws2812_rtio_config *config = dev->config;
//   const struct ws2812_rtio_data *data = dev->data;
//   uint8_t *ptr = (uint8_t *)data->pixels;
//   size_t p;
//   // int rc;

//   /* Convert from RGB to on-wire format (e.g. GRB, GRBW, RGB, etc) */
//   for (p = 0; p < data->num_pixels; p++) {
//     uint8_t c;

//     for (c = 0; c < config->num_colors; c++) {
//       switch (config->color_mapping[c]) {
//         /* White channel is not supported by LED strip API. */
//         case LED_COLOR_ID_WHITE:
//           *ptr++ = 0;
//           break;
//         case LED_COLOR_ID_RED:
//           *ptr++ = data->pixels[p].r;
//           break;
//         case LED_COLOR_ID_GREEN:
//           *ptr++ = data->pixels[p].g;
//           break;
//         case LED_COLOR_ID_BLUE:
//           *ptr++ = data->pixels[p].b;
//           break;
//         default:
//           break;
//       }
//     }
//   }

//   // rc = send_buf(dev, (uint8_t *)pixels, num_pixels * config->num_colors);
// }

static void ws2812_rtio_update_rgb(
    const struct device *dev, struct led_rgb *pixels
) {
  const struct ws2812_rtio_config *config = dev->config;
  uint8_t *ptr = (uint8_t *)pixels;
  size_t p;

  /* Convert from RGB to on-wire format (e.g. GRB, GRBW, RGB, etc) */
  for (p = 0; p < config->chain_length; p++) {
    uint8_t c;

    for (c = 0; c < config->num_colors; c++) {
      switch (config->color_mapping[c]) {
        /* White channel is not supported by LED strip API. */
        case LED_COLOR_ID_WHITE:
          *ptr++ = 0;
          break;
        case LED_COLOR_ID_RED:
          *ptr++ = 0x33;  // pixels[p].r;
          break;
        case LED_COLOR_ID_GREEN:
          *ptr++ = 0x33;  // pixels[p].g;
          break;
        case LED_COLOR_ID_BLUE:
          *ptr++ = 0x33;  // pixels[p].b;
          break;
        default:
          break;
      }
    }
  }
}

static int ws2812_rtio_iodev_write(
    const struct device *dev, struct led_rgb *pixels
) {
  LOG_DBG("Entering ws2812_rtio_iodev_write");
  const struct ws2812_rtio_config *config = dev->config;
  // struct ws2812_rtio_data *data = dev->data;
  size_t bit;
  uint32_t key;
  int offset = 0;
  uint8_t *ptr = (uint8_t *)pixels;
  size_t len = config->chain_length;

  // LOG_DBG("%s: buf_len = %d, buf = %p", dev->name, len, pixels);

  // LOG_DBG("\nIRQ lock");

  key = irq_lock();
  while (len--) {
    for (bit = 0; bit < 8; bit++) {
      if (0b10000000 & (*(ptr + offset) << bit)) {
        gpio_pin_set_dt(&config->gpios, 1);
        gpio_pin_set_dt(&config->gpios, 1);
        gpio_pin_set_dt(&config->gpios, 0);
      } else {
        gpio_pin_set_dt(&config->gpios, 1);
        gpio_pin_set_dt(&config->gpios, 0);
        gpio_pin_set_dt(&config->gpios, 0);
      }
    }
    offset++;
  }
  irq_unlock(key);
  // LOG_DBG("IRQ unlock");

  return 0;
}

static void ws2812_rtio_iodev_execute(
    const struct device *dev, struct rtio_iodev_sqe *iodev_sqe,
    struct led_rgb *pixels
) {
  LOG_DBG("Entering ws2812_rtio_iodev_execute");
  // const struct ws2812_rtio_config *config = dev->config;
  int result;

  if (iodev_sqe->sqe.op == RTIO_OP_TX) {
    result = ws2812_rtio_iodev_write(dev, pixels);
  } else {
    LOG_ERR("%s: Invalid op", dev->name);
    result = -EINVAL;
  }

  if (result < 0) {
    rtio_iodev_sqe_err(iodev_sqe, result);
  } else {
    rtio_iodev_sqe_ok(iodev_sqe, result);
  }
}

static void ws2812_rtio_iodev_submit(struct rtio_iodev_sqe *iodev_sqe) {
  LOG_DBG("Entering ws2812_rtio_iodev_submit");
  struct ws2812_rtio_data *data =
      (struct ws2812_rtio_data *)iodev_sqe->sqe.iodev;

  rtio_mpsc_push(&data->iodev.iodev_sq, &iodev_sqe->q);
}

static void ws2812_rtio_handle_int(const struct device *dev) {
  LOG_DBG("Entering ws2812_rtio_handle_int");
  struct ws2812_rtio_data *data = dev->data;
  const struct ws2812_rtio_config *config = dev->config;
  struct rtio_mpsc_node *node = rtio_mpsc_pop(&data->iodev.iodev_sq);
  struct led_rgb pixels[config->chain_length];

  if (node != NULL) {
    struct rtio_iodev_sqe *iodev_sqe =
        CONTAINER_OF(node, struct rtio_iodev_sqe, q);

    ws2812_rtio_update_rgb(dev, pixels);

    ws2812_rtio_iodev_execute(dev, iodev_sqe, pixels);
  } else {
    LOG_ERR("%s: Could not get a msg", dev->name);

    /* FOR DEBUGGING ONLY*/
    exit(1);
  }
}

static void ws2812_rtio_timer_expiry(struct k_timer *timer) {
  LOG_DBG("Entering ws2812_rtio_timer_expiry");
  struct ws2812_rtio_data *data =
      CONTAINER_OF(timer, struct ws2812_rtio_data, timer);

  ws2812_rtio_handle_int(data->dev);
}

// static int ws2812_rtio_update_channels(
//     const struct device *dev, uint8_t *channels, size_t num_channels
// ) {
//   LOG_ERR("update_channels not implemented");
//   return -ENOTSUP;
// }

// static const struct led_strip_driver_api ws2812_rtio_api = {
//     .update_rgb = ws2812_rtio_update_rgb,
//     .update_channels = ws2812_rtio_update_channels
// };

static const struct rtio_iodev_api ws2812_rtio_iodev_api = {
    .submit = ws2812_rtio_iodev_submit,
};

static int ws2812_rtio_init(const struct device *dev) {
  LOG_DBG("Entering ws2812_rtio_init");
  const struct ws2812_rtio_config *config = dev->config;
  struct ws2812_rtio_data *data = dev->data;
  int err = 0;
  uint8_t c;

  if (!gpio_is_ready_dt(&config->gpios)) {
    LOG_ERR("GPIO device not ready");
    return -ENODEV;
  }

  err = gpio_pin_configure_dt(&config->gpios, GPIO_OUTPUT_LOW);
  if (err) {
    LOG_ERR("Failed to configure gpio");
    return err;
  }

  for (c = 0; c < config->num_colors; c++) {
    switch (config->color_mapping[c]) {
      case LED_COLOR_ID_WHITE:
      case LED_COLOR_ID_RED:
      case LED_COLOR_ID_GREEN:
      case LED_COLOR_ID_BLUE:
        break;
      default:
        LOG_ERR(
            "%s: invalid channel to color mapping."
            " Check the color-mapping DT property",
            dev->name
        );
        return -EINVAL;
    }
  }

  // Start accurate HFCLK (XOSC)
  // NRF_CLOCK->TASKS_HFCLKSTART = 1;
  // LOG_WRN("HFCLK started");

  data->dev = dev;
  // data->num_pixels = config->chain_length;
  // data->num_pixels = config->chain_length * config->num_colors;
  LOG_DBG("num_pixels: %i", config->chain_length);

  rtio_mpsc_init(&data->iodev.iodev_sq);
  k_timer_init(&data->timer, ws2812_rtio_timer_expiry, NULL);
  k_timer_start(&data->timer, K_MSEC(15), K_NSEC(PULSE_WIDTH_NS));

  return 0;
}

/*
 * Retrieve the channel to color mapping (e.g. RGB, BGR, GRB, ...) from the
 * "color-mapping" DT property.
 */
#define WS2812_COLOR_MAPPING(idx)                            \
  static const uint8_t ws2812_rtio_##idx##_color_mapping[] = \
      DT_INST_PROP(idx, color_mapping)

#define WS2812_NUM_COLORS(idx) (DT_INST_PROP_LEN(idx, color_mapping))

#define WS2812_RTIO_DEVICE(idx)                                         \
  WS2812_COLOR_MAPPING(idx);                                            \
                                                                        \
  static const struct ws2812_rtio_config ws2812_rtio_##idx##_config = { \
      .gpios = GPIO_DT_SPEC_INST_GET(idx, gpios),                       \
      .num_colors = WS2812_NUM_COLORS(idx),                             \
      .color_mapping = ws2812_rtio_##idx##_color_mapping,               \
      .chain_length = DT_INST_PROP(idx, chain_length)                   \
  };                                                                    \
                                                                        \
  static struct ws2812_rtio_data ws2812_rtio_##idx##_data = {           \
      .iodev =                                                          \
          {                                                             \
              .api = &ws2812_rtio_iodev_api,                            \
          },                                                            \
  };                                                                    \
                                                                        \
  DEVICE_DT_INST_DEFINE(                                                \
      idx, ws2812_rtio_init, NULL, &ws2812_rtio_##idx##_data,           \
      &ws2812_rtio_##idx##_config, POST_KERNEL,                         \
      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL                          \
  );

DT_INST_FOREACH_STATUS_OKAY(WS2812_RTIO_DEVICE)
