#ifndef REAL_TIME_COUNTER_H
#define REAL_TIME_COUNTER_H
int set_rtc_time(void);
unsigned int get_rtc_time(void);

extern struct k_sem rtc_sync_sem;
#endif  // REAL_TIME_COUNTER_H
