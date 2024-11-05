#ifndef APP_RTC_H
#define APP_RTC_H
int set_app_rtc_time(void);
unsigned int get_app_rtc_time(void);

extern struct k_sem rtc_sync_sem;
#endif
