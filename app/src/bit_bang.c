#include "bit_bang.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/led/led.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/sys/sys_io.h>

// #include "zephyr/drivers/led_strip.h"

LOG_MODULE_REGISTER(bbt, LOG_LEVEL_DBG);

#define N (2)
#define M (N / 2)
#define SQ_SZ (N)
#define CQ_SZ (N)

#define NODE_ID DT_ALIAS(led_strip)
#define SAMPLE_PERIOD 250
#define SAMPLE_SIZE 24
#define PROCESS_TIME ((M - 1) * SAMPLE_PERIOD)
#define BIT_LEN 24
#define PULSE_WIDTH_NS 250

RTIO_DEFINE(ws2812_io, SQ_SZ, CQ_SZ);

int bit_bang_test(void) {
  const struct device *const ws2812_rtio = DEVICE_DT_GET(NODE_ID);
  struct rtio_iodev *iodev = ws2812_rtio->data;
  uint8_t *buf[M] = {0};
  uint32_t len[M] = {0};

  /* Fill the entire submission queue. */
  for (int n = 0; n < M; n++) {
    struct rtio_sqe *sqe = rtio_sqe_acquire(&ws2812_io);

    rtio_sqe_prep_write(sqe, iodev, RTIO_PRIO_HIGH, buf[n], len[n], NULL);
  }

  // while (true) {
  int m = 0;

  LOG_INF("Submitting %d write requests", M);
  rtio_submit(&ws2812_io, M);

  /* Consume completion events until there is enough sensor data
   * available to execute a batch processing algorithm, such as
   * an FFT.
   */
  while (m < M) {
    struct rtio_cqe *cqe = rtio_cqe_consume(&ws2812_io);

    if (cqe == NULL) {
      LOG_DBG("No completion events available");
      k_msleep(SAMPLE_PERIOD);
      continue;
    }
    LOG_DBG("Consumed completion event %d", m);

    if (cqe->result < 0) {
      LOG_ERR("Operation failed");
    }

    // if (rtio_cqe_get_mempool_buffer(
    //         &ws2812_io, cqe, &userdata[m], &data_len[m]
    //     )) {
    //   LOG_ERR("Failed to get mempool buffer info");
    // }
    rtio_cqe_release(&ws2812_io, cqe);
    m++;
  }

  /* Here is where we would execute a batch processing algorithm.
   * Model as a long sleep that takes multiple sensor sample
   * periods. The sensor driver can continue reading new data
   * during this time because we submitted more buffers into the
   * queue than we needed for the batch processing algorithm.
   */
  LOG_INF("Start processing %d samples", M);
  for (m = 0; m < M; m++) {
    LOG_HEXDUMP_DBG(buf[m], SAMPLE_SIZE, "Sample data:");
  }
  k_msleep(PROCESS_TIME);
  LOG_INF("Finished processing %d samples", M);

  /* Recycle the sensor data buffers and refill the submission
   * queue.
   */
  for (m = 0; m < M; m++) {
    struct rtio_sqe *sqe = rtio_sqe_acquire(&ws2812_io);

    rtio_release_buffer(&ws2812_io, &buf[m], len[m]);
    rtio_sqe_prep_write(sqe, iodev, RTIO_PRIO_HIGH, buf[m], M, NULL);
  }
  // }
  return 0;

  // // LOG_DBG("Start bit-banging");
  // int err;
  // unsigned int key;

  // if (!gpio_is_ready_dt(&bit_bang)) {
  //   printf("The load switch pin GPIO port is not ready.\n");
  //   return 1;
  // }

  // LOG_DBG("Initializing pin with inactive level.\n");

  // err = gpio_pin_configure_dt(&bit_bang, GPIO_OUTPUT_INACTIVE);
  // if (err != 0) {
  //   LOG_ERR("Configuring GPIO pin failed: %d", err);
  //   return 1;
  // }

  // key = irq_lock();

  // LOG_DBG("Waiting one us.");
  // k_busy_wait(2);

  // LOG_DBG("Setting pin to high level.\n");
  // err = gpio_pin_set_dt(&bit_bang, 1);
  // if (err != 0) {
  //   LOG_ERR("Setting GPIO pin level failed: %d\n", err);
  //   return 1;
  // }

  // LOG_DBG("Waiting one us.");
  // k_busy_wait(2);

  // LOG_DBG("Setting pin to low level.\n");
  // err = gpio_pin_set_dt(&bit_bang, 0);
  // if (err != 0) {
  //   LOG_ERR("Setting GPIO pin level failed: %d\n", err);
  //   return 1;
  // }

  // irq_unlock(key);

  return 0;
}
