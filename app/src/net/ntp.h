#ifndef NTP_H
#define NTP_H

#include <zephyr/net/sntp.h>

extern struct sntp_time time_stamp;

/** @fn int get_ntp_time(void)
 *  @brief Get NTP time usiing Zephyr's sntp client.
 */
int get_ntp_time(void);

#endif  // NTP_H
