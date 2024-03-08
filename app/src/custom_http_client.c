/** @headerfile custom_http_client.h */
#include <custom_http_client.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

LOG_MODULE_REGISTER(custom_http_client, LOG_LEVEL_DBG);

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)
static K_SEM_DEFINE(network_connected_sem, 0, 1);

/** A macro that defines the HTTP request host name for the request headers. */
#define STOP_REQUEST_HOSTNAME "bustracker.pvta.com"

/** A macro that defines the HTTP file path for the request headers. */
#define HTTP_REQUEST_PATH "/stop/" STOP_ID

/** A macro that defines the HTTP port for the request headers. */
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#define HTTP_PORT "443"
#else
#define HTTP_PORT "80"
#endif

#define TLS_SEC_TAG 42

/** @def HTTP_REQUEST_HEADERS
 *  @brief A macro that defines the HTTP request headers for the GET request.
 */
#define HTTP_REQUEST_HEADERS                   \
  "GET " HTTP_REQUEST_PATH                     \
  " HTTP/1.1\r\n"                              \
  "Host: " STOP_REQUEST_HOSTNAME ":" HTTP_PORT \
  "\r\n"                                       \
  "Accept: application/json\r\n"               \
  "Connection: close\r\n\r\n"

/** @def RECV_HEADER_BUF_SIZE
 *  @brief A macro that defines the max size for HTTP response headers receive
 * buffer.
 *
 *  Seems to be ~320 bytes, size ~doubled for safety.
 */
#define RECV_HEADER_BUF_SIZE 600

#define HTTP_REQUEST_HEAD_LEN (sizeof(HTTP_REQUEST_HEADERS) - 1)

/** HTTP send buffer defined by the HTTP_REQUEST_HEADERS macro. */
static const char send_buf[] = HTTP_REQUEST_HEADERS;

/** HTTP response headers buffer with size defined by the RECV_HEADER_BUF_SIZE
 * macro. */
static char recv_headers_buf[RECV_HEADER_BUF_SIZE] = "\0";

/** HTTP response body buffer with size defined by the RECV_BODY_BUF_SIZE macro.
 */
char recv_body_buf[RECV_BODY_BUF_SIZE] = "\0";

static int parse_status(void) {
  char *ptr;
  int code;

  ptr = strstr(recv_headers_buf, "HTTP");
  if (ptr == NULL) {
    LOG_ERR("Missing HTTP status line!");
    return -1;
  }

  ptr = strstr(recv_headers_buf, " ");
  if (ptr == NULL) {
    LOG_ERR("Improperly formatted HTTP status line!");
    return -1;
  }

  code = atoi(ptr);

  if ((code < 100) || (code > 511)) {
    LOG_ERR("HTTP status code missing or incorrect!");
    return -1;
  }

  return code;
}

static int separate_headers(int *sock) {
  int state = 0;
  int bytes;
  size_t offset = 0;
  bool headers = true;

  do {
    if (headers) {
      bytes = zsock_recv(*sock, &recv_headers_buf[offset], 1, 0);
      if (bytes < 0) {
        LOG_ERR("recv() headers failed, err %d\n", errno);
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
      bytes = zsock_recv(
          *sock, &recv_body_buf[offset], RECV_BODY_BUF_SIZE - offset, 0
      );
      if (bytes < 0) {
        LOG_ERR("recv() body failed, err %d\n", errno);
        return bytes;
      }
    }
    offset += bytes;
  } while (bytes != 0); /* peer closed connection */

  LOG_INF("Received %d bytes", offset);

  /* Make sure recv_buf is NULL terminated (for safe use with strstr) */
  if (offset < sizeof(recv_body_buf)) {
    recv_body_buf[offset] = '\0';
  } else {
    recv_body_buf[sizeof(recv_body_buf) - 1] = '\0';
  }

  return 0;
}

/* Setup TLS options on a given socket */
int tls_setup(int fd) {
  int err;
  int verify;

  /* Security tag that we have provisioned the certificate with */
  const sec_tag_t tls_sec_tag[] = {
      TLS_SEC_TAG,
  };

  /* Set up TLS peer verification */
  enum {
    NONE = 0,
    OPTIONAL = 1,
    REQUIRED = 2,
  };

  verify = REQUIRED;

  err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
  if (err) {
    LOG_ERR("Failed to setup peer verification, err %d\n", errno);
    return err;
  }

  /* Associate the socket with the security tag
   * we have provisioned the certificate with.
   */
  err = setsockopt(
      fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag)
  );
  if (err) {
    LOG_ERR("Failed to setup TLS sec tag, err %d\n", errno);
    return err;
  }

  err = setsockopt(
      fd, SOL_TLS, TLS_HOSTNAME, STOP_REQUEST_HOSTNAME,
      sizeof(STOP_REQUEST_HOSTNAME) - 1
  );
  if (err) {
    LOG_ERR("Failed to setup TLS hostname, err %d\n", errno);
    return err;
  }
  return 0;
}

static int send_http_request(void) {
  int attempt = 0;
  int bytes;
  int err;
  int fd;
  size_t offset;
  int sock = -1;
  int status = -1;

  struct addrinfo *addr_inf;
  static struct addrinfo hints = {
      .ai_socktype = SOCK_STREAM, .ai_flags = AI_NUMERICSERV
  };

  err = getaddrinfo(STOP_REQUEST_HOSTNAME, NULL, &hints, &addr_inf);
  if (err) {
    LOG_ERR("getaddrinfo() failed, err %d\n", errno);
    return errno;
  }

  // ((struct sockaddr_in *)addr_inf->ai_addr)->sin_port = htons(HTTP_PORT);

  char peer_addr[INET6_ADDRSTRLEN];

  inet_ntop(
      addr_inf->ai_family,
      &((struct sockaddr_in *)(addr_inf->ai_addr))->sin_addr, peer_addr,
      INET6_ADDRSTRLEN
  );

  if (IS_ENABLED(CONFIG_MBEDTLS)) {
    fd = socket(
        addr_inf->ai_family, SOCK_STREAM | SOCK_NATIVE_TLS, IPPROTO_TLS_1_2
    );
  } else {
    fd = socket(addr_inf->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
  }
  if (fd == -1) {
    LOG_ERR("Failed to open socket!\n");
    goto clean_up;
  }

retry:
  sock = zsock_socket(AF_INET, SOCK_STREAM, addr_inf->ai_protocol);
  if (sock == -1) {
    LOG_ERR("Failed to open socket!\n");
    goto clean_up;
  }

  /* Setup TLS socket options */
  err = tls_setup(fd);
  if (err) {
    goto clean_up;
  }

  err = connect(fd, addr_inf->ai_addr, addr_inf->ai_addrlen);
  if (err) {
    LOG_ERR("connect() failed, err: %d\n", errno);
    goto clean_up;
  }

  offset = 0;
  do {
    bytes = send(sock, &send_buf[offset], HTTP_REQUEST_HEAD_LEN - offset, 0);
    if (bytes < 0) {
      LOG_ERR("send() failed, err %d.", errno);
      attempt++;
      goto clean_up;
    }
    offset += bytes;
  } while (offset < HTTP_REQUEST_HEAD_LEN);

  LOG_INF("Sent %d bytes", offset);

  if (separate_headers(&sock) < 0) {
    LOG_ERR("EOF or error in response headers.");
    attempt++;
    goto clean_up;
  }

  status = parse_status();
  /* Sometimes the server returns a 404 for seemingly no reason, so we retry. */
  if (status != 200) {
    if (attempt < RETRY_COUNT) {
      LOG_WRN("Bad HTTP response code: %d. Retrying...", status);
      LOG_DBG("\nResponse Headers:\n%s\n", recv_headers_buf);
      attempt++;
      goto clean_up;
    }
    LOG_ERR("Failed to get JSON, bad HTTP response code: %d.", status);
  }

clean_up:
  (void)zsock_close(sock);

  if ((attempt > 0) && (attempt <= RETRY_COUNT) && (status != 200)) {
    goto retry;
  }

  zsock_freeaddrinfo(addr_inf);

  return status;
}

int http_request_stop_json(void) {
  return send_http_request();

  // /* A small delay for the TCP connection teardown */
  // k_sleep(K_SECONDS(1));

  // /* The HTTP transaction is done, take the network connection down */
  // err = conn_mgr_all_if_disconnect(true);
  // if (err) {
  //   LOG_INF("conn_mgr_all_if_disconnect, error: %d\n", err);
  // }

  // err = conn_mgr_all_if_down(true);
  // if (err) {
  //   LOG_INF("conn_mgr_all_if_down, error: %d\n", err);
  // }

  // return 0;
}
