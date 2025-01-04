#include "ntp.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ntp);

#define SNTP_PORT "123"

struct sntp_time time_stamp;

static int ntp_request(char *url) {
  int err;
  struct sntp_ctx ctx;

  struct zsock_addrinfo *addr_inf;
  static struct zsock_addrinfo hints = {.ai_socktype = SOCK_DGRAM, .ai_flags = AI_NUMERICSERV};

  err = zsock_getaddrinfo(url, SNTP_PORT, &hints, &addr_inf);
  if (err) {
    LOG_ERR("getaddrinfo() failed, %s", strerror(errno));
    return errno;
  }

  if (addr_inf->ai_family == AF_INET) {
    err = sntp_init(&ctx, addr_inf->ai_addr, sizeof(struct sockaddr_in));
  } else {
    err = sntp_init(&ctx, addr_inf->ai_addr, sizeof(struct sockaddr_in6));
  }

  if (err < 0) {
    LOG_ERR("Failed to init SNTP ctx: %d", err);
    goto end;
  }

  err = sntp_query(&ctx, CONFIG_NTP_REQUEST_TIMEOUT_MS, &time_stamp);
  if (err) {
    LOG_ERR("SNTP request failed: %d", err);
  }

end:
  sntp_close(&ctx);
  return err;
}

int get_ntp_time(void) {
  int err;

  /* Get sntp time */
  for (int rc = 0; rc < (CONFIG_NTP_FETCH_RETRY_COUNT * 2); rc++) {
    if (rc < CONFIG_NTP_FETCH_RETRY_COUNT) {
      err = ntp_request(CONFIG_PRIMARY_NTP_SERVER);
    } else {
      err = ntp_request(CONFIG_FALLBACK_NTP_SERVER);
    }

    if (err && (rc == (CONFIG_NTP_FETCH_RETRY_COUNT * 2) - 1)) {
      LOG_ERR(
          "Failed to get time from all NTP pools! Err: %i\n Check your network "
          "connection.",
          err
      );
    } else if (err && (rc == CONFIG_NTP_FETCH_RETRY_COUNT - 1)) {
      LOG_WRN(
          "Unable to get time after %d tries from NTP "
          "pool " CONFIG_PRIMARY_NTP_SERVER " . Err: %i\n Attempting to use fallback NTP pool...",
          CONFIG_NTP_FETCH_RETRY_COUNT, err
      );
    } else if (err) {
      LOG_WRN("Failed to get time using SNTP, Err: %i. Retrying...", err);
    } else {
      break;
    }
  }
  return err;
}
