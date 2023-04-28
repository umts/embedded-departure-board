#ifndef RTC_H
#define RTC_H

#include <zephyr/sys/timeutil.h>

void rtc_init(void);
void set_rtc_time(void);
int get_rtc_time(void);
#endif
