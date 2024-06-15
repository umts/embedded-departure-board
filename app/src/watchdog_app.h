#ifndef WATCHDOG_APP_H
#define WATCHDOG_APP_H

#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>

extern const struct device *const wdt;
extern int wdt_channel_id;

int watchdog_init(void);

#endif
