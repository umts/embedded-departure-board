/** @headerfile custom_http_client.h */
#include <custom_http_client.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
// #include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
// #include <zephyr/net/tls_credentials.h>

#include <zephyr/logging/log.h>

/* Newlib C includes */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* nrf lib includes */
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

LOG_MODULE_REGISTER(custom_http_client, LOG_LEVEL_DBG);

/** A macro that defines the HTTP request host name for the request headers. */
#define HTTP_REQUEST_HOSTNAME "bustracker.pvta.com"

/** A macro that defines the HTTP file path for the request headers. */
#define HTTP_REQUEST_PATH \
  "/InfoPoint/rest/SignageStopDepartures/GetSignageDeparturesByStopId?stopId=" STOP_ID

/** A macro that defines the HTTP port for the request headers. */
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
  #define HTTP_PORT "443"
#else
  #define HTTP_PORT "80"
#endif

/** @def HTTP_REQUEST_HEADERS
 *  @brief A macro that defines the HTTP request headers for the GET request.
*/
#define HTTP_REQUEST_HEADERS \
  "GET " HTTP_REQUEST_PATH " HTTP/1.1\r\n" \
  "Host: " HTTP_REQUEST_HOSTNAME ":" HTTP_PORT "\r\n" \
  "Accept: application/json\r\n" \
  "Connection: close\r\n\r\n"

/** @def RECV_HEADER_BUF_SIZE
 *  @brief A macro that defines the max size for HTTP response headers receive buffer.
 *
 *  Seems to be ~320 bytes, size ~doubled for safety.
*/
#define RECV_HEADER_BUF_SIZE 600

#define HTTP_REQUEST_HEAD_LEN (sizeof(HTTP_REQUEST_HEADERS) - 1)

/** HTTP send buffer defined by the HTTP_REQUEST_HEADERS macro. */
static const char send_buf[] = HTTP_REQUEST_HEADERS;

/** HTTP response headers buffer with size defined by the RECV_HEADER_BUF_SIZE macro. */
static char recv_headers_buf[RECV_HEADER_BUF_SIZE];

/** HTTP response body buffer with size defined by the RECV_BODY_BUF_SIZE macro. */
char recv_body_buf[RECV_BODY_BUF_SIZE];

/** Godaddy server ca certificate */
// static const char ca_certificate[] = {
// 	#include "../cert/gdig2.crt.pem"
// };

static int connect_socket(int *sock, struct addrinfo *addr_inf) {
	int ret;
  static struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	ret = getaddrinfo(HTTP_REQUEST_HOSTNAME, NULL, &hints, &addr_inf);
	if (ret) {
		LOG_ERR("getaddrinfo() failed, err %d\n", errno);
	}

	((struct sockaddr_in *)addr_inf->ai_addr)->sin_port = htons(80);

	*sock = socket(AF_INET, SOCK_STREAM, addr_inf->ai_protocol);

	if (*sock == -1) {
		LOG_ERR("Failed to open socket!\n");
	}

	ret = connect(*sock, addr_inf->ai_addr, sizeof(struct sockaddr_in));
	if (ret) {
		LOG_ERR("connect() failed, err: %d\n", errno);
	}

	return ret;
}

static int parse_status(bool *redirect) {
	char *ptr;
	int code;

	ptr = strstr(recv_body_buf, "HTTP");
	if (ptr == NULL) {
		return -1;
	}

	ptr = strstr(recv_body_buf, " ");
	if (ptr == NULL) {
		return -1;
	}

	ptr++;

	code = atoi(ptr);
	if (code >= 300 && code < 400) {
		*redirect = true;
	}

	return 0;
}

int skip_headers(int *sock) {
	int state = 0;
  int bytes;
  size_t offset = 0;
  bool headers = true;

	do {
    if (headers) {
		  bytes = recv(*sock, &recv_headers_buf[offset], 1, 0);
      if (bytes < 0) {
        LOG_ERR("recv() failed, err %d\n", errno);
        return bytes;
      }

      if ((state == 0 || state == 2) && recv_headers_buf[offset] == '\r') {
        state++;
      } else if (state == 1 && recv_headers_buf[offset] == '\n') {
        state++;
      } else if (state == 3) {
        headers = false;
        recv_headers_buf[offset + 1] = '\0';
        offset = 0;
        continue;
      } else {
        state = 0;
      }
    } else {
      bytes = recv(*sock, &recv_body_buf[offset], RECV_BODY_BUF_SIZE - offset, 0);
      if (bytes < 0) {
        LOG_ERR("recv() failed, err %d\n", errno);
        return bytes;
      }
    }
    offset += bytes;
	} while (bytes != 0); /* peer closed connection */

  LOG_INF("Received %d bytes\n", offset);

  /* Make sure recv_buf is NULL terminated (for safe use with strstr) */
	if (offset < sizeof(recv_body_buf)) {
		recv_body_buf[offset] = '\0';
	} else {
		recv_body_buf[sizeof(recv_body_buf) - 1] = '\0';
	}

	return 0;
}

int http_request_json(void) {
	int sock = -1;
  bool redirect = false;
	// char *response_body = NULL;
	int bytes;
	size_t offset = 0;
	struct addrinfo *addr_inf;

  if (connect_socket(&sock, addr_inf) < 0) { goto clean_up; }

	do {
		bytes = send(sock, &send_buf[offset], HTTP_REQUEST_HEAD_LEN - offset, 0);
		if (bytes < 0) {
			LOG_ERR("send() failed, err %d\n", errno);
			goto clean_up;
		}
		offset += bytes;
	} while (offset < HTTP_REQUEST_HEAD_LEN);

	LOG_INF("Sent %d bytes\n", offset);

  if (skip_headers(&sock) < 0) {
		LOG_ERR("EOF or error in response headers\n");
	}

clean_up:
	LOG_INF("Finished, closing socket.\n");
	// freeaddrinfo(res);
	(void)close(sock);

  return 0;
}
