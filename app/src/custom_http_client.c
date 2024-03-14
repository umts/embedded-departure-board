/** @headerfile custom_http_client.h */
#include <custom_http_client.h>
#include <lte_manager.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_intsup.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

LOG_MODULE_REGISTER(custom_http_client, LOG_LEVEL_DBG);

/** A macro that defines the HTTP request host name for the request headers. */
#define STOP_REQUEST_HOSTNAME "nidd.jes.contact"

/** A macro that defines the HTTP file path for the request headers. */
#define STOP_REQUEST_PATH "/stop/" STOP_ID

/** A macro that defines the HTTP request host name for the request headers. */
#define FIRMWARE_REQUEST_HOSTNAME "api.github.com"

// TEMP
#define ASSET_ID "151564620"

/** A macro that defines the HTTP file path for the request headers. */
#define FIRMWARE_REQUEST_PATH \
  "/repos/umts/embedded-departure-board/releases/assets/" ASSET_ID

/** @def RECV_HEADER_BUF_SIZE
 *  @brief A macro that defines the max size for HTTP headers
 * buffer.
 */
#define HEADER_BUF_SIZE 1024

/** HTTP headers buffer with size defined by the
 * HEADER_BUF_SIZE macro. */
static char headers_buf[HEADER_BUF_SIZE] = "\0";

/** HTTP response body buffer with size defined by the
 * RECV_BODY_BUF_SIZE macro.
 */
char recv_body_buf[RECV_BODY_BUF_SIZE] = "\0";

/* Setup TLS options on a given socket */
int tls_setup(int fd, char *hostname, sec_tag_t sec_tag) {
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

  err = setsockopt(
      fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag)
  );
  if (err) {
    LOG_ERR("Failed to setup TLS sec tag, err %d\n", errno);
    return err;
  }

  err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME, hostname, strlen(hostname));
  if (err) {
    LOG_ERR("Failed to setup TLS hostname, err %d\n", errno);
    return err;
  }
  return 0;
}

static int parse_status(void) {
  char *ptr;
  int code;

  ptr = strstr(headers_buf, "HTTP");
  if (ptr == NULL) {
    LOG_ERR("Missing HTTP status line");
    return HTTP_NULL;
  }

  ptr = strstr(headers_buf, " ");
  if (ptr == NULL) {
    LOG_ERR("Improperly formatted HTTP status line");
    return HTTP_NULL;
  }

  code = atoi(ptr);

  switch (code) {
    case 100 ... 199:
      LOG_INF("Informational response code: %d", code);
      return HTTP_INFO;
    case 200 ... 299:
      LOG_INF("Successful response code: %d", code);
      return HTTP_SUCCESS;
    case 300 ... 399:
      LOG_INF("Redirection response code: %d", code);
      return HTTP_REDIRECT;
    case 400 ... 499:
      LOG_INF("Client error response code: %d", code);
      return HTTP_CLIENT_ERROR;
    case 500 ... 599:
      LOG_INF("Server error response code: %d", code);
      return HTTP_SERVER_ERROR;
    default:
      LOG_ERR("HTTP status code missing or incorrect. Response code: %d", code);
      return HTTP_NULL;
  }
}

static int parse_response(int *sock) {
  int state = 0;
  int bytes;
  size_t offset = 0;
  bool headers = true;
  int status;
  size_t headers_size = 0;

  do {
    if (headers) {
      bytes = recv(*sock, &headers_buf[offset], 1, 0);
      if (bytes < 0) {
        LOG_ERR("recv() headers failed, err %d\n", errno);
        return bytes;
      }

      if ((state == 0 || state == 2) && headers_buf[offset] == '\r') {
        state++;
      } else if (state == 1 && headers_buf[offset] == '\n') {
        state++;
      } else if (state == 3) {
        headers_size = offset;
        LOG_INF("Received Headers. Size: %d bytes", offset);

        headers = false;
        headers_buf[offset + 1] = '\0';
        offset = 0;
        continue;
      } else {
        state = 0;
      }
    } else {
      status = parse_status();
      switch (status) {
        case HTTP_REDIRECT:
          return 1;
        case HTTP_CLIENT_ERROR:
          return -1;
        case HTTP_SERVER_ERROR:
          return -1;
        case HTTP_NULL:
          return -1;
        default:
          break;
      }

      bytes =
          recv(*sock, &recv_body_buf[offset], RECV_BODY_BUF_SIZE - offset, 0);
      if (bytes < 0) {
        LOG_ERR("recv() body failed, err %d\n", errno);
        return bytes;
      }
    }
    offset += bytes;
  } while (bytes != 0); /* peer closed connection */

  LOG_INF("Received Body. Size: %d bytes", offset);
  LOG_INF("Total bytes received: %d", offset + headers_size);

  /* Make sure recv_buf is NULL terminated (for safe use with strstr) */
  if (offset < sizeof(recv_body_buf)) {
    recv_body_buf[offset] = '\0';
  } else {
    recv_body_buf[sizeof(recv_body_buf) - 1] = '\0';
  }

  return 0;
}

static char *get_redirect_location(void) {
  char *ptr;

  ptr = strstr(headers_buf, "Location:");
  if (ptr == NULL) {
    ptr = strstr(headers_buf, "location:");
    if (ptr == NULL) {
      LOG_ERR("Location header missing");
      return NULL;
    }
  }

  ptr += 9;

  while (*ptr == ' ') {
    ptr++;
  }

  *strstr(ptr, "\r\n") = '\0';

  return ptr;
}

static int send_http_request(
    char *hostname, char *path, char *accept, sec_tag_t sec_tag
) {
  int bytes;
  int err;
  int headers_size;
  size_t offset;
  char *ptr;
  int redirect;
  int sock = -1;

  struct addrinfo *addr_inf;
  static struct addrinfo hints = {
      .ai_socktype = SOCK_STREAM, .ai_flags = AI_NUMERICSERV
  };

  LOG_DBG("hostname: %s", hostname);
  LOG_DBG("path: %s", path);
  LOG_DBG("accept: %s", accept);
  LOG_DBG("SEC_TAG: %d", sec_tag);

retry:
  ptr = stpcpy(&headers_buf[0], "GET ");
  ptr = stpcpy(ptr, path);
  ptr = stpcpy(ptr, " HTTP/1.1\r\n");
  ptr = stpcpy(ptr, "Host: ");
  ptr = stpcpy(ptr, hostname);
  if (sec_tag == NO_SEC_TAG) {
    ptr = stpcpy(ptr, ":80\r\n");
  } else {
    ptr = stpcpy(ptr, ":443\r\n");
  }
  ptr = stpcpy(ptr, "Accept: ");
  ptr = stpcpy(ptr, accept);
  ptr = stpcpy(ptr, "\r\nConnection: close\r\n\r\n");

  headers_size = (ptr - &headers_buf[0]);

  LOG_DBG("Send Headers (size: %d):\n%s", headers_size, &headers_buf[0]);

  if (sec_tag == NO_SEC_TAG) {
    err = getaddrinfo(hostname, "80", &hints, &addr_inf);
  } else {
    err = getaddrinfo(hostname, "443", &hints, &addr_inf);
  }
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

  if (IS_ENABLED(CONFIG_MBEDTLS)) {
    sock = socket(
        addr_inf->ai_family, SOCK_STREAM | SOCK_NATIVE_TLS, IPPROTO_TLS_1_2
    );
  } else {
    sock = socket(addr_inf->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
  }
  if (sock == -1) {
    LOG_ERR("Failed to open socket!\n");
    goto clean_up;
  }

  /* Setup TLS socket options */
  err = tls_setup(sock, hostname, sec_tag);
  if (err) {
    goto clean_up;
  }

  LOG_DBG(
      "Connecting to %s:%d", hostname,
      ntohs(((struct sockaddr_in *)(addr_inf->ai_addr))->sin_port)
  );
  err = connect(sock, addr_inf->ai_addr, addr_inf->ai_addrlen);
  if (err) {
    LOG_ERR("connect() failed, err: %d\n", errno);
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
    bytes = send(sock, &headers_buf[offset], headers_size - offset, 0);
    if (bytes < 0) {
      LOG_ERR("send() failed, err %d.", errno);
      goto clean_up;
    }
    offset += bytes;
  } while (offset < headers_size);

  LOG_INF("Sent %d bytes", offset);

  redirect = parse_response(&sock);
  if (redirect < 0) {
    LOG_ERR("EOF or error in response headers.");
  }

clean_up:
  LOG_INF("Closing socket %d", sock);
  err = close(sock);
  if (err) {
    LOG_ERR("close() failed, err: %d\n", errno);
  }

  (void)freeaddrinfo(addr_inf);

  LOG_DBG("Response Headers:\n%s", &headers_buf[0]);

  if (redirect == 1) {
    goto redirect;
  }

  LOG_DBG("Response Body:\n%s", &recv_body_buf[0]);
  return 0;

redirect:
  ptr = get_redirect_location();
  LOG_DBG("redirect ptr: %s", ptr);

  /* Assume the host is the same */
  if (*ptr == '/') {
    path = strcpy(&recv_body_buf[0], ptr);
    LOG_DBG("path ptr: %s", path);
    /* Assume we're dealing with a url */
  } else if (*ptr == 'h') {
    if (strncmp(ptr, "https", 5) == 0) {
      ptr += 8;
    } else {
      ptr += 7;
      sec_tag = NO_SEC_TAG;
    }
    hostname = ptr;
    ptr = strstr(ptr, "/");
    *ptr++ = '\0';

    path = stpcpy(&recv_body_buf[0], hostname);
    *++path = '/';

    (void)strcpy((path + 1), ptr);
    hostname = &recv_body_buf[0];
  } else {
    LOG_ERR("Bad redirect location");
    return 1;
  }

  goto retry;
}

int http_request_stop_json(void) {
  int err;

  if (k_sem_take(&lte_connected_sem, K_SECONDS(30)) != 0) {
    LOG_ERR("Failed to take lte_connected_sem");
    err = 1;
  } else {
    err = send_http_request(
        STOP_REQUEST_HOSTNAME, STOP_REQUEST_PATH, "application/json",
        JES_SEC_TAG
    );
    k_sem_give(&lte_connected_sem);
  }

  return err;
}

int http_get_firmware(void) {
  int err;

  if (k_sem_take(&lte_connected_sem, K_SECONDS(30)) != 0) {
    LOG_ERR("Failed to take lte_connected_sem");
    err = 1;
  } else {
    err = send_http_request(
        FIRMWARE_REQUEST_HOSTNAME, FIRMWARE_REQUEST_PATH,
        "application/octet-stream,text/html;charset=utf-8", GITHUB_SEC_TAG
    );
    k_sem_give(&lte_connected_sem);
  }

  return err;
}
