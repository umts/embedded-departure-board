#ifdef CONFIG_LIGHT_SENSOR
#include "light_sensor.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(light_sensor);

#define I2C1 DT_NODELABEL(i2c1)

#if DT_NODE_HAS_STATUS(I2C1, okay)
const struct device *const i2c_dev = DEVICE_DT_GET(I2C1);
#else
#error "i2c1 node is disabled"
#endif

#define SEN_ADDR 0x23
#define SEN_REG_LUX 0x10

int get_lux(void) {
  uint8_t lux[2] = {0, 0};
  int ret;

  ret = i2c_burst_read(i2c_dev, SEN_ADDR, SEN_REG_LUX, &lux[0], 2);
  if (ret) {
    LOG_WRN("Unable get lux data. Keeping PWM lights off. (err %i)", ret);
    return 0xFF;
  }

  ret = ((uint16_t)lux[0] << 8) | (uint16_t)lux[1];
  LOG_INF("LUX: 0x%x", ret);

  return ret;
}
#endif  // CONFIG_LIGHT_SENSOR
