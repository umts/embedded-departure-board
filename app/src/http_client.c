#include <string.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <zephyr/net/socket.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#define HTTP_HOSTNAME "bustracker.pvta.com"
#define STOP_ID "73"

#define HTTP_HEAD \
  "GET /InfoPoint/rest/StopDepartures/Get/" STOP_ID " HTTP/1.1\r\n" \
	"Host: " HTTP_HOSTNAME ":80\r\n" \
  "Accept: application/json\r\n" \
	"Connection: close\r\n\r\n"

#define HTTP_HEAD_LEN (sizeof(HTTP_HEAD) - 1)

#define RECV_BUF_SIZE 40960

static const char send_buf[] = HTTP_HEAD;
static char recv_buf[RECV_BUF_SIZE];

/* Initialize AT communications */
int at_comms_init(void) {
	int err;

	err = at_cmd_init();
	if (err) {
		printk("Failed to initialize AT commands, err %d\n", err);
		return err;
	}

	err = at_notif_init();
	if (err) {
		printk("Failed to initialize AT notifications, err %d\n", err);
		return err;
	}

	return 0;
}


char* http_get_request(void)
{
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

	err = lte_lc_init_and_connect();
	if (err) {
		printk("Failed to connect to the LTE network, err %d\n", err);
		return;
	}

	err = getaddrinfo(HTTP_HOSTNAME, NULL, &hints, &res);
	if (err) {
		printk("getaddrinfo() failed, err %d\n", errno);
		return;
	}

	((struct sockaddr_in *)res->ai_addr)->sin_port = htons(80);

	fd = socket(AF_INET, SOCK_STREAM, 0);//, IPPROTO_TLS_1_2);
	
	if (fd == -1) {
		printk("Failed to open socket!\n");
		goto clean_up;
	}

	err = connect(fd, res->ai_addr, sizeof(struct sockaddr_in));
	if (err) {
		printk("connect() failed, err: %d\n", errno);
		goto clean_up;
	}

	off = 0;
	do {
		bytes = send(fd, &send_buf[off], HTTP_HEAD_LEN - off, 0);
		if (bytes < 0) {
			printk("send() failed, err %d\n", errno);
			goto clean_up;
		}
		off += bytes;
	} while (off < HTTP_HEAD_LEN);

	printk("Sent %d bytes\n", off);

	off = 0;
	do {
		bytes = recv(fd, &recv_buf[off], RECV_BUF_SIZE - off, 0);
		if (bytes < 0) {
			printk("recv() failed, err %d\n", errno);
			goto clean_up;
		}
		off += bytes;
	} while (bytes != 0 /* peer closed connection */);

	printk("Received %d bytes\n", off);

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
	printk("Finished, closing socket.\n");
	freeaddrinfo(res);
	(void)close(fd);

	lte_lc_power_off();
  return(response_body);
}