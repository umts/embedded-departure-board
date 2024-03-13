/** @headerfile custom_http_client.h */
#include <custom_http_client.h>
#include <lte_manager.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

LOG_MODULE_REGISTER(custom_http_client, LOG_LEVEL_DBG);

// static K_SEM_DEFINE(network_connected_sem, 0, 1);
#define ASSET_ID "151564620"

/** A macro that defines the HTTP request host name for the request headers. */
#define STOP_REQUEST_HOSTNAME "nidd.jes.contact"

/** A macro that defines the HTTP file path for the request headers. */
#define STOP_REQUEST_PATH "/stop/" STOP_ID

/** A macro that defines the HTTP request host name for the request headers. */
#define FIRMWARE_REQUEST_HOSTNAME "api.github.com"

/** A macro that defines the HTTP file path for the request headers. */
#define FIRMWARE_REQUEST_PATH \
  "/repos/umts/embedded-departure-board/releases/assets/" ASSET_ID

/** A macro that defines the HTTP port for the request headers. */
#define HTTP_PORT "443"

/** @def STOP_REQUEST_HEADERS
 *  @brief A macro that defines the HTTP request headers for the GET request.
 */
#define STOP_REQUEST_HEADERS                   \
  "GET " STOP_REQUEST_PATH                     \
  " HTTP/1.1\r\n"                              \
  "Host: " STOP_REQUEST_HOSTNAME ":" HTTP_PORT \
  "\r\n"                                       \
  "Accept: application/json\r\n"               \
  "Connection: close\r\n\r\n"

/** @def FIRMWARE_REQUEST_HEADERS
 *  @brief A macro that defines the HTTP request headers for the GET request.
 */
#define FIRMWARE_REQUEST_HEADERS                   \
  "GET " FIRMWARE_REQUEST_PATH                     \
  " HTTP/1.1\r\n"                                  \
  "Host: " FIRMWARE_REQUEST_HOSTNAME ":" HTTP_PORT \
  "\r\n"                                           \
  "Accept: application/octet-stream\r\n"           \
  "X-GitHub-Api-Version: 2022-11-28\r\n"           \
  "User-Agent: NCS/2.5.2\r\n"                      \
  "Connection: close\r\n\r\n"

/** @def RECV_HEADER_BUF_SIZE
 *  @brief A macro that defines the max size for HTTP response headers receive
 * buffer.
 *
 *  Seems to be ~320 bytes, size ~doubled for safety.
 */
#define RECV_HEADER_BUF_SIZE 600

#define HTTP_REQUEST_HEAD_LEN(h_) (sizeof(h_) - 1)

/** HTTP response headers buffer with size defined by the
 * RECV_HEADER_BUF_SIZE macro. */
static char recv_headers_buf[RECV_HEADER_BUF_SIZE] = "\0";

/** HTTP response body buffer with size defined by the
 * RECV_BODY_BUF_SIZE macro.
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
      bytes = recv(*sock, &recv_headers_buf[offset], 1, 0);
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
      bytes =
          recv(*sock, &recv_body_buf[offset], RECV_BODY_BUF_SIZE - offset, 0);
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

  LOG_DBG("Response Headers:\n%s\n", &recv_headers_buf[0]);
  LOG_DBG("Response Body:\n%s\n", &recv_body_buf[0]);

  return 0;
}

/* Setup TLS options on a given socket */
int tls_setup(int fd, sec_tag_t sec_tag) {
  int err;
  int verify;

  /* Security tag that we have provisioned the certificate with */
  sec_tag_t tls_sec_tag[] = {sec_tag};

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

static int send_http_request(
    char *hostname, char *send_buf, int headers_size, sec_tag_t sec_tag
) {
  int attempt = 0;
  int bytes;
  int err;
  size_t offset;
  int sock = -1;
  int status = -1;

  struct addrinfo *addr_inf;
  static struct addrinfo hints = {
      .ai_socktype = SOCK_STREAM, .ai_flags = AI_NUMERICSERV
  };

  err = getaddrinfo(hostname, HTTP_PORT, &hints, &addr_inf);
  if (err) {
    LOG_ERR("getaddrinfo() failed, err %d\n", errno);
    return errno;
  }

  char peer_addr[INET6_ADDRSTRLEN];

  if (inet_ntop(
          addr_inf->ai_family,
          &((struct sockaddr_in *)(addr_inf->ai_addr))->sin_addr, peer_addr,
          INET6_ADDRSTRLEN
      ) == NULL) {
    LOG_ERR("inet_ntop() failed, err %d\n", errno);
    return errno;
  }

  LOG_INF("Resolved %s (%s)", peer_addr, net_family2str(addr_inf->ai_family));

retry:
  if (IS_ENABLED(CONFIG_MBEDTLS)) {
    sock = socket(
        addr_inf->ai_family, SOCK_STREAM | SOCK_NATIVE_TLS, IPPROTO_TLS_1_2
    );
  } else {
    sock = socket(addr_inf->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
  }
  if (sock == -1) {
    LOG_ERR("Failed to open socket!\n");
    attempt++;
    goto clean_up;
  }

  /* Setup TLS socket options */
  err = tls_setup(sock, sec_tag);
  if (err) {
    attempt++;
    goto clean_up;
  }

  LOG_DBG(
      "Connecting to %s:%d\n", hostname,
      ntohs(((struct sockaddr_in *)(addr_inf->ai_addr))->sin_port)
  );
  err = connect(sock, addr_inf->ai_addr, addr_inf->ai_addrlen);
  if (err) {
    LOG_ERR("connect() failed, err: %d\n", errno);
    attempt++;
    goto clean_up;
  }

  LOG_DBG(
      "addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
      "sa_family=%d, sin_port=%x\n",
      addr_inf, addr_inf->ai_family, addr_inf->ai_socktype,
      addr_inf->ai_protocol, addr_inf->ai_addr->sa_family,
      ((struct sockaddr_in *)addr_inf->ai_addr)->sin_port
  );

  offset = 0;
  do {
    bytes = send(sock, &send_buf[offset], headers_size - offset, 0);
    if (bytes < 0) {
      LOG_ERR("send() failed, err %d.", errno);
      attempt++;
      goto clean_up;
    }
    offset += bytes;
  } while (offset < headers_size);

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
    LOG_ERR("Failed to get body, bad HTTP response code: %d.", status);
  }

clean_up:
  LOG_INF("Closing socket %d", sock);
  err = close(sock);
  if (err) {
    LOG_ERR("close() failed, err: %d\n", errno);
    attempt++;
    goto clean_up;
  }

  if ((attempt > 0) && (attempt <= RETRY_COUNT) && (status != 200)) {
    goto retry;
  }

  (void)freeaddrinfo(addr_inf);

  return status;
}

int http_request_stop_json(void) {
  int err;
  enum tls_sec_tags sec_tag = JES_SEC_TAG;

  /** HTTP send buffer defined by the HTTP_REQUEST_HEADERS macro. */
  char send_buf[] = STOP_REQUEST_HEADERS;
  char hostname[] = STOP_REQUEST_HOSTNAME;

  LOG_DBG("SEC_TAG: %d", sec_tag);
  LOG_DBG("HOSTNAME: %s", hostname);
  LOG_DBG("HEADERS_SIZE: %d", HTTP_REQUEST_HEAD_LEN(STOP_REQUEST_HEADERS));
  LOG_DBG("Send buf:\n%s\n", &send_buf[0]);

  if (k_sem_take(&lte_connected_sem, K_SECONDS(30)) != 0) {
    LOG_ERR("Failed to take lte_connected_sem");
    err = 1;
  } else {
    err = send_http_request(
        hostname, send_buf, HTTP_REQUEST_HEAD_LEN(STOP_REQUEST_HEADERS), sec_tag
    );
    k_sem_give(&lte_connected_sem);
  }

  return err;
}

int http_get_firmware(void) {
  int err;
  enum tls_sec_tags sec_tag = GITHUB_SEC_TAG;

  char send_buf[] = FIRMWARE_REQUEST_HEADERS;
  char hostname[] = FIRMWARE_REQUEST_HOSTNAME;

  LOG_DBG("SEC_TAG: %d", sec_tag);
  LOG_DBG("HOSTNAME: %s", hostname);
  LOG_DBG("HEADERS_SIZE: %d", HTTP_REQUEST_HEAD_LEN(FIRMWARE_REQUEST_HEADERS));
  LOG_DBG("Send buf:\n%s\n", &send_buf[0]);

  if (k_sem_take(&lte_connected_sem, K_SECONDS(30)) != 0) {
    LOG_ERR("Failed to take lte_connected_sem");
    err = 1;
  } else {
    err = send_http_request(
        hostname, send_buf, HTTP_REQUEST_HEAD_LEN(FIRMWARE_REQUEST_HEADERS),
        sec_tag
    );
    k_sem_give(&lte_connected_sem);
  }

  return err;
}
