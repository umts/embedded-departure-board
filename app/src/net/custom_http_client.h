/** @file custom_http_client.h
 *  @brief Macros and function defines for the HTTP client.
 */

#ifndef CUSTOM_HTTP_CLIENT_H
#define CUSTOM_HTTP_CLIENT_H

/** @brief Makes an HTTP GET request and returns a char pointer to the HTTP
 * response body buffer.
 */
int http_request_stop_json(
    char *stop_body_buf, int stop_body_buf_size, char *headers_buf, int headers_buf_size
);

#endif  // CUSTOM_HTTP_CLIENT_H
