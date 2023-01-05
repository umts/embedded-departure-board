#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H
  #include <stop.h>
  #define HTTP_HOSTNAME "bustracker.pvta.com"
  #define HTTP_HEAD \
    "GET /InfoPoint/rest/StopDepartures/Get/" STOP_ID " HTTP/1.1\r\n" \
    "Host: " HTTP_HOSTNAME ":80\r\n" \
    "Accept: application/json\r\n" \
    "Connection: close\r\n\r\n"
  #define HTTP_HEAD_LEN (sizeof(HTTP_HEAD) - 1)
  #define RECV_BUF_SIZE 40960

  char* http_get_request(void);
#endif