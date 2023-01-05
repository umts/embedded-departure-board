#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <http_client.h>

LOG_MODULE_REGISTER(http_client, LOG_LEVEL_DBG);

static const char send_buf[] = HTTP_HEAD;
static char recv_buf[RECV_BUF_SIZE];

char* http_get_request(void) {
	int err;
	int fd;
	char *response_body;
	int bytes;
	size_t off;
	struct addrinfo *res;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	err = getaddrinfo(HTTP_HOSTNAME, NULL, &hints, &res);
	if (err) {
		LOG_ERR("getaddrinfo() failed, err %d\n", errno);
	}

	((struct sockaddr_in *)res->ai_addr)->sin_port = htons(80);

	fd = socket(AF_INET, SOCK_STREAM, 0);//, IPPROTO_TLS_1_2);
	
	if (fd == -1) {
		LOG_ERR("Failed to open socket!\n");
		goto clean_up;
	}

	err = connect(fd, res->ai_addr, sizeof(struct sockaddr_in));
	if (err) {
		LOG_ERR("connect() failed, err: %d\n", errno);
		goto clean_up;
	}

	off = 0;
	do {
		bytes = send(fd, &send_buf[off], HTTP_HEAD_LEN - off, 0);
		if (bytes < 0) {
			LOG_ERR("send() failed, err %d\n", errno);
			goto clean_up;
		}
		off += bytes;
	} while (off < HTTP_HEAD_LEN);

	LOG_INF("Sent %d bytes\n", off);

	off = 0;
	do {
		bytes = recv(fd, &recv_buf[off], RECV_BUF_SIZE - off, 0);
		if (bytes < 0) {
			LOG_ERR("recv() failed, err %d\n", errno);
			goto clean_up;
		}
		off += bytes;
	} while (bytes != 0 /* peer closed connection */);

	LOG_INF("Received %d bytes\n", off);

	/* Make sure recv_buf is NULL terminated (for safe use with strstr) */
	if (off < sizeof(recv_buf)) {
		recv_buf[off] = '\0';
	} else {
		recv_buf[sizeof(recv_buf) - 1] = '\0';
	}

	/* Seperate body from HTTP response */
	response_body = strstr(recv_buf, "\r\n\r\n") + 4;
	// if (response_headers) {
	// 	off = recv_buf - response_headers;
	// 	recv_buf[off + 1] = '\0';
	// }

clean_up:
	LOG_INF("Finished, closing socket.\n");
	freeaddrinfo(res);
	(void)close(fd);

  return(response_body);
}