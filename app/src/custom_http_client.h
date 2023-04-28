/** @file custom_http_client.h
 *  @brief Macros and function defines for the HTTP client.
 */

#ifndef CUSTOM_HTTP_CLIENT_H
#define CUSTOM_HTTP_CLIENT_H
#include <stop.h>

/** @def RECV_BODY_BUF_SIZE
 *  @brief A macro that defines the max size for HTTP response body receive buffer.
 *
 *  Actual size varies quite a bit depending on how many routes are currently running.
 */
#define RECV_BODY_BUF_SIZE 10240

/** HTTP response body buffer with size defined by the RECV_BODY_BUF_SIZE macro. */
extern char recv_body_buf[RECV_BODY_BUF_SIZE];

/** @fn char* http_get_request(void)
 *  @brief Makes an HTTP GET request and returns a char pointer to the HTTP response body buffer.
 */
int http_request_json(void);
#endif
