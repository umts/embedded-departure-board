/** @file custom_http_client.h
 *  @brief Macros and function defines for the HTTP client.
 */

#ifndef CUSTOM_HTTP_CLIENT_H
#define CUSTOM_HTTP_CLIENT_H

enum response_code {
  HTTP_NULL,
  HTTP_INFO,
  HTTP_SUCCESS,
  HTTP_REDIRECT,
  HTTP_CLIENT_ERROR,
  HTTP_SERVER_ERROR
};

/** @brief Makes an HTTP GET request and returns a char pointer to the HTTP
 * response body buffer.
 */
int http_request_stop_json(
    char *stop_body_buf, int stop_body_buf_size, char *headers_buf,
    int headers_buf_size
);

#ifdef CONFIG_JES_FOTA
/** @brief Makes an HTTP GET request to download a firmware update file and
 * writes it to flash.
 */
int http_get_firmware(
    char *write_buf, int write_buf_size, char *headers_buf, int headers_buf_size
);
#endif  // CONFIG_JES_FOTA

#endif  // CUSTOM_HTTP_CLIENT_H
