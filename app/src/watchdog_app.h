#ifndef WATCHDOG_APP_H
#define WATCHDOG_APP_H

#include <zephyr/kernel.h>

extern const struct device *const wdt;

int watchdog_init(void);

#endif
