/** @headerfile http_client.h */
#include <custom_http_client.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>

#include <zephyr/logging/log.h>

/* Newlib C includes */
#include <stdlib.h>
#include <string.h>

/* nrf lib includes */
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#define CA_CERTIFICATE_TAG 1

LOG_MODULE_REGISTER(http_client, LOG_LEVEL_DBG);

/** HTTP send buffer defined by the HTTP_REQUEST_HEADERS macro. */
static const char send_buf[] = HTTP_REQUEST_HEADERS;

/** HTTP response headers buffer with size defined by the RECV_HEADER_BUF_SIZE macro. */
// static char recv_headers_buf[RECV_HEADER_BUF_SIZE];

/** HTTP response body buffer with size defined by the RECV_BODY_BUF_SIZE macro. */
static char recv_body_buf[RECV_BODY_BUF_SIZE];

static char download_url[] = "http://" HTTP_REQUEST_HOSTNAME HTTP_REQUEST_PATH;

/* Certificate for godaddy */
static const char ca_certificate[] = {
	#include "../cert/gdig2.crt.pem"
};

void dump_addrinfo(struct addrinfo *ai) {
  LOG_WRN(
    "addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
    "sa_family=%d, sin_port=%x\n",
    ai, ai->ai_family, ai->ai_socktype, ai->ai_protocol,
    ai->ai_addr->sa_family,
    ((struct sockaddr_in *)ai->ai_addr)->sin_port
  );
}

static int setup_socket(
  sa_family_t family, const char *server, int port,
	int *sock, struct sockaddr *addr, socklen_t addr_len
) {
	const char *family_str = "IPv4";
	int ret = 0;

	memset(addr, 0, addr_len);

  net_sin(addr)->sin_family = AF_INET;
  net_sin(addr)->sin_port = htons(port);
  inet_pton(family, server, &net_sin(addr)->sin_addr);

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		sec_tag_t sec_tag_list[] = {
			CA_CERTIFICATE_TAG,
		};

		*sock = socket(family, SOCK_STREAM, IPPROTO_TLS_1_2);
		if (*sock >= 0) {
			ret = setsockopt(*sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list, sizeof(sec_tag_list));
			if (ret < 0) {
				LOG_ERR("Failed to set %s secure option (%d)", family_str, -errno);
				ret = -errno;
			}

			ret = setsockopt(*sock, SOL_TLS, TLS_HOSTNAME, HTTP_REQUEST_HOSTNAME, sizeof(HTTP_REQUEST_HOSTNAME));
			if (ret < 0) {
				LOG_ERR("Failed to set %s TLS_HOSTNAME option (%d)", family_str, -errno);
				ret = -errno;
			}
		}
	} else {
		*sock = socket(family, SOCK_STREAM, IPPROTO_TCP);
	}

	if (*sock < 0) {
		LOG_ERR("Failed to create %s HTTP socket (%d)", family_str, -errno);
	}

  LOG_WRN(
    "addrinfo @%p: ai_family=%d, ai_socktype=%d, "
    "ai_protocol=%d, sa_family=%d, sin_port=%x\n",
    &net_sin(addr)->sin_addr, net_sin(addr)->sin_family, SOCK_STREAM,
    IPPROTO_TCP, family, net_sin(addr)->sin_port
  );

	return ret;
}

static int connect_socket(sa_family_t family, const char *server, int port,
	int *sock, struct sockaddr *addr, socklen_t addr_len
) {
	int ret;

	ret = setup_socket(family, server, port, sock, addr, addr_len);
	if (ret < 0 || *sock < 0) {
		return -1;
	}

	ret = connect(*sock, addr, addr_len);
	if (ret < 0) {
	  LOG_ERR("Cannot connect to IPv4 remote (%d)", -errno);
		ret = -errno;
	}

	return ret;
}

// static int parse_header(bool *location_found {
// 	char *ptr;

// 	ptr = strstr(recv_body_buf, ":");
// 	if (ptr == NULL) {
// 		return 0;
// 	}

// 	*ptr = '\0';
// 	ptr = recv_body_buf;

// 	while (*ptr != '\0') {
// 		*ptr = tolower(*ptr);
// 		ptr++;
// 	}

// 	if (strcmp(recv_body_buf, "location") != 0) {
// 		return 0;
// 	}

// 	/* Skip whitespace */
// 	while (*(++ptr) == ' ') {
// 		;
// 	}

// 	strncpy(download_url, ptr, sizeof(download_url));
// 	download_url[sizeof(download_url) - 1] = '\0';

// 	/* Trim LF. */
// 	ptr = strstr(download_url, "\n");
// 	if (ptr == NULL) {
// 		LOG_ERR("Redirect URL too long or malformed\n");
// 		return -1;
// 	}

// 	*ptr = '\0';

// 	/* Trim CR if present. */
// 	ptr = strstr(download_url, "\r");
// 	if (ptr != NULL) {
// 		*ptr = '\0';
// 	}

// 	*location_found = true;

// 	return 0;
// }

int skip_headers(int sock, bool *redirect) {
	int state = 0;
	int i = 0;
	bool status_line = true;
	bool redirect_code = false;
	bool location_found = false;

	while (1) {
		int st;

		st = recv(sock, recv_body_buf + i, 1, 0);
		if (st <= 0) {
			return st;
		}

		if (state == 0 && recv_body_buf[i] == '\r') {
			state++;
		} else if ((state == 0 || state == 1) && recv_body_buf[i] == '\n') {
			state = 2;
			recv_body_buf[i + 1] = '\0';
			i = 0;

			if (status_line) {
				if (parse_status(&redirect_code) < 0) {
					return -1;
				}

				status_line = false;
			} else {
				if (parse_header(&location_found) < 0) {
					return -1;
				}
			}

			continue;
		} else if (state == 2 && recv_body_buf[i] == '\r') {
			state++;
		} else if ((state == 2 || state == 3) && recv_body_buf[i] == '\n') {
			break;
		} else {
			state = 0;
		}

		i++;
		if (i >= sizeof(recv_body_buf) - 1) {
			i = 0;
		}
	}

	if (redirect_code && location_found) {
		*redirect = true;
	}

	return 1;
}

static void response_cb(struct http_response *rsp,
  enum http_final_call final_data, void *user_data
) {
	if (final_data == HTTP_DATA_MORE) {
		LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		LOG_INF("All the data received (%zd bytes)", rsp->data_len);
	}

	LOG_INF("Response to %s", (const char *)user_data);
	LOG_INF("Response status %s", rsp->http_status);
}


int run_queries(void) {
	struct sockaddr_in addr4;
	int sock4 = -1;
	int ret = 0;
	int port = 80;
  int32_t timeout = 3 * MSEC_PER_SEC;

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		ret = tls_credential_add(
      CA_CERTIFICATE_TAG,
			TLS_CREDENTIAL_CA_CERTIFICATE,
			ca_certificate,
			sizeof(ca_certificate)
    );
		if (ret < 0) {
			LOG_ERR("Failed to register public certificate: %d", ret);
			return ret;
		}

		port = 443;
	}

  (void)connect_socket(
    AF_INET, HTTP_REQUEST_HOSTNAME, port, &sock4, (struct sockaddr *)&addr4, sizeof(addr4)
  );

	if (sock4 < 0) {
		LOG_ERR("Cannot create HTTP connection.");
		return -ECONNABORTED;
	}

  struct http_request req;

  memset(&req, 0, sizeof(req));

	if (sock4 >= 0) {
		struct http_request req;

		memset(&req, 0, sizeof(req));

		req.method = HTTP_GET;
		req.url = "/";
		req.host = HTTP_REQUEST_HOSTNAME;
		req.protocol = "HTTP/1.1";
		req.response = response_cb;
		req.recv_buf = recv_body_buf;
		req.recv_buf_len = sizeof(recv_body_buf);

		ret = http_client_req(sock4, &req, timeout, "IPv4 GET");

		close(sock4);
	}
	return ret;
}

char* http_request_json(void) {
	int err;
	int fd;
	char *response_body = NULL;
	int bytes;
	size_t off;
	struct addrinfo *res;
	static struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	err = getaddrinfo(HTTP_REQUEST_HOSTNAME, NULL, &hints, &res);
	if (err) {
		LOG_ERR("getaddrinfo() failed, err %d\n", errno);
	}

	((struct sockaddr_in *)res->ai_addr)->sin_port = htons(80);

	fd = socket(AF_INET, SOCK_STREAM, res->ai_protocol);

	if (fd == -1) {
		LOG_ERR("Failed to open socket!\n");
		goto clean_up;
	}

  dump_addrinfo(res);

	err = connect(fd, res->ai_addr, sizeof(struct sockaddr_in));
	if (err) {
		LOG_ERR("connect() failed, err: %d\n", errno);
		goto clean_up;
	}

	off = 0;
	do {
		bytes = send(fd, &send_buf[off], sizeof(HTTP_REQUEST_HEADERS) - off, 0);
		if (bytes < 0) {
			LOG_ERR("send() failed, err %d\n", errno);
			goto clean_up;
		}
		off += bytes;
	} while (off < sizeof(HTTP_REQUEST_HEADERS));

	LOG_INF("Sent %d bytes\n", off);

	off = 0;
	do {
		bytes = recv(fd, &recv_body_buf[off], RECV_BODY_BUF_SIZE - off, 0);
		if (bytes < 0) {
			LOG_ERR("recv() failed, err %d\n", errno);
			goto clean_up;
		}
		off += bytes;
	} while (bytes != 0 /* peer closed connection */);

	LOG_INF("Received %d bytes\n", off);

	/* Make sure recv_buf is NULL terminated (for safe use with strstr) */
	if (off < sizeof(recv_body_buf)) {
		recv_body_buf[off] = '\0';
	} else {
		recv_body_buf[sizeof(recv_body_buf) - 1] = '\0';
	}

	/* Seperate body from HTTP response */
	response_body = strstr(recv_body_buf, "\r\n\r\n") + 4;

clean_up:
	LOG_INF("Finished, closing socket.\n");
	freeaddrinfo(res);
	(void)close(fd);

  return response_body;
}
