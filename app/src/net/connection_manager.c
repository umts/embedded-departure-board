/** @headerfile connection_manager.h */
#include "connection_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>

#ifdef CONFIG_MODEM_KEY_MGMT
#include <modem/modem_key_mgmt.h>
#else
#include <zephyr/net/tls_credentials.h>
#endif

#include "watchdog_app.h"

LOG_MODULE_REGISTER(connection_manager);

#if defined(CONFIG_JES_FOTA) || defined(CONFIG_STOP_REQUEST_JES)
static const char jes_cert[] = {
#include "jes-contact-root-r4.pem.hex"
    // Null terminate certificate if running Mbed TLS
    IF_ENABLED(CONFIG_TLS_CREDENTIALS, (0x00))
};
#endif  // CONFIG_JES_FOTA || CONFIG_STOP_REQUEST_JES

#ifdef CONFIG_MODEM_KEY_MGMT
// The total size of the included certificates must be less than 4KB
#if defined(CONFIG_JES_FOTA) || defined(CONFIG_STOP_REQUEST_JES)
BUILD_ASSERT(sizeof(jes_cert) < KB(4), "Certificates too large");
#endif  // CONFIG_JES_FOTA || CONFIG_STOP_REQUEST_JES
#endif

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

K_SEM_DEFINE(network_connection_sem, 1, 1);

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

/* Provision certificate to modem */
#ifdef CONFIG_MODEM_KEY_MGMT
static int provision_cert(nrf_sec_tag_t sec_tag, const char cert[], size_t cert_len) {
  int err;

  bool exists;
  int mismatch;

  err = modem_key_mgmt_exists(sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists);
  if (err) {
    LOG_ERR("Failed to check for certificates err %d", err);
    return err;
  }

  if (exists) {
    mismatch = modem_key_mgmt_cmp(sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert, cert_len);
    if (!mismatch) {
      LOG_INF("Certificate match");
      return 0;
    }

    LOG_INF("Certificate mismatch");
    err = modem_key_mgmt_delete(sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
    if (err) {
      LOG_ERR("Failed to delete existing certificate, err %d", err);
    }
  }

  LOG_INF("Provisioning certificate");

  /*  Provision certificate to the modem */
  err = modem_key_mgmt_write(sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert, cert_len);
  if (err) {
    LOG_ERR("Failed to provision certificate, err %d", err);
    return err;
  }

  return 0;
}
#else
static int provision_cert(sec_tag_t sec_tag, const char cert[], size_t cert_len) {
  int err = tls_credential_add(sec_tag, TLS_CREDENTIAL_CA_CERTIFICATE, cert, cert_len);
  if (err == -EEXIST) {
    LOG_INF("CA certificate already exists, sec tag: %d", sec_tag);
  } else if (err < 0) {
    LOG_ERR("Failed to register CA certificate: %d", err);
    return err;
  }
  return 0;
}
#endif /* !CONFIG_MODEM_KEY_MGMT */

static void on_net_event_l4_disconnected(void) { LOG_INF("Disconnected from the network"); }

static void on_net_event_l4_connected(void) { k_sem_give(&network_connection_sem); }

static void l4_event_handler(
    struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface
) {
  switch (event) {
    case NET_EVENT_L4_CONNECTED:
      LOG_INF("Network connectivity established and IP address assigned");
      on_net_event_l4_connected();
      break;
    case NET_EVENT_L4_DISCONNECTED:
      LOG_WRN("Network connectivity lost");
      on_net_event_l4_disconnected();
      break;
    default:
      break;
  }
}

static void connectivity_event_handler(
    struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface
) {
  if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
    LOG_ERR("Fatal error received from the connectivity layer");
    return;
  }
}

int lte_connect(void) {
  int err;

  /* Setup handler for Zephyr NET Connection Manager events. */
  net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
  net_mgmt_add_event_callback(&l4_cb);

  /* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
  net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
  net_mgmt_add_event_callback(&conn_cb);

  LOG_INF("Bringing network interface up");

  k_sem_take(&network_connection_sem, K_FOREVER);

  err = wdt_feed(wdt, wdt_channel_id);
  if (err) {
    LOG_ERR("Failed to feed watchdog. Err: %d", err);
    return 1;
  }

  /* Connecting to the configured connectivity layer. */
  err = conn_mgr_all_if_up(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_up, error: %d", err);
    return err;
  }

  /* Provision certificates before connecting to the network */
#if defined(CONFIG_JES_FOTA) || defined(CONFIG_STOP_REQUEST_JES)
  err = provision_cert(JES_SEC_TAG, jes_cert, sizeof(jes_cert));
  if (err) {
    LOG_ERR("Failed to provision TLS certificate. TLS_SEC_TAG: %d", JES_SEC_TAG);
    return err;
  }
#endif  // CONFIG_JES_FOTA || CONFIG_STOP_REQUEST_JES

  LOG_INF("Connecting to the network");

  err = conn_mgr_all_if_connect(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_connect, error: %d", err);
    return 0;
  }

  return 0;
}

int lte_disconnect(void) {
  int err;

  /* A small delay for the TCP connection teardown */
  k_sleep(K_SECONDS(1));

  /* The HTTP transaction is done, take the network connection down */
  err = conn_mgr_all_if_disconnect(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_disconnect, error: %d", err);
  }

  err = conn_mgr_all_if_down(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_down, error: %d", err);
  }

  return err;
}
