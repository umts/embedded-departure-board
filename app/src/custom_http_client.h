/** @file custom_http_client.h
 *  @brief Macros and function defines for the HTTP client.
 */

#ifndef CUSTOM_HTTP_CLIENT_H
#define CUSTOM_HTTP_CLIENT_H
#include <stop.h>

enum response_code {
  HTTP_NULL,
  HTTP_INFO,
  HTTP_SUCCESS,
  HTTP_REDIRECT,
  HTTP_CLIENT_ERROR,
  HTTP_SERVER_ERROR
};

/** @def RECV_BODY_BUF_SIZE
 *  @brief A macro that defines the max size for HTTP response body receive buffer.
 *
 *  Actual size varies quite a bit depending on how many routes are currently running.
 */
#define STOP_BODY_BUF_SIZE 10240

/** @fn char* http_get_request(void)
 *  @brief Makes an HTTP GET request and returns a char pointer to the HTTP response body buffer.
 */
int http_request_stop_json(char *stop_body_buf, int stop_body_buf_size);

int http_get_firmware(void);
#endif
