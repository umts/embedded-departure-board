#include <zephyr/kernel.h>
#include <stdlib.h>
#include <stdio.h>
//#include <https_client.h>
#include <http_client.h>
#include <zephyr/drivers/i2c.h>
//#include <data/json.h>
//#include <parse_json.h>
//#include <rtc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/counter.h>
#include <drivers/counter/pcf85063a.h>

#define MY_I2C "I2C_1"
const struct device *i2c_dev;

void main(void) {
  //rtc_init();
  pcf85063a_init(const struct device *dev);
  k_msleep(1000);
  printk("Ready");
  //printk("Time: %i", get_time());
  //printk("\n%s", http_get_request());
  //parse_json();
  
  // // Bind i2c
	// i2c_dev = device_get_binding(MY_I2C);
	// if (i2c_dev == NULL) {
	// 	printk("Can't bind I2C device %s\n", MY_I2C);
	// 	return;
	// }

	// // Write to i2c
	// uint8_t i2c_addr = 0x41;
  // unsigned char i2c_tx_buffer[] = {'3', '0'};
  // i2c_write(i2c_dev, &i2c_tx_buffer, sizeof(i2c_tx_buffer), i2c_addr);
}