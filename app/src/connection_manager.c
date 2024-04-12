/** @headerfile connection_manager.h */
#include "connection_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>

#if CONFIG_MODEM_KEY_MGMT
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <modem/nrf_modem_lib.h>
#else
#include <zephyr/drivers/cellular.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>

const struct device *modem = DEVICE_DT_GET(DT_ALIAS(modem));
#endif

LOG_MODULE_REGISTER(connection_manager, LOG_LEVEL_DBG);

#define TLS_SEC_TAG 42

static const char ca_cert[] = {
#include "../keys/public/jes-contact.pem"
};

BUILD_ASSERT(sizeof(ca_cert) < KB(4), "Certificate too large");

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

K_SEM_DEFINE(network_connected_sem, 0, 1);

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

/* Provision certificate to modem */
int cert_provision(void) {
  int err;

#if CONFIG_MODEM_KEY_MGMT
  bool exists;
  int mismatch;

  /* It may be sufficient for you application to check whether the correct
   * certificate is provisioned with a given tag directly using
   * modem_key_mgmt_cmp(). Here, for the sake of the completeness, we check that
   * a certificate exists before comparing it with what we expect it to be.
   */
  err = modem_key_mgmt_exists(
      TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists
  );
  if (err) {
    LOG_ERR("Failed to check for certificates err %d\n", err);
    return err;
  }

  if (exists) {
    mismatch = modem_key_mgmt_cmp(
        TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, ca_cert, sizeof(ca_cert)
    );
    if (!mismatch) {
      LOG_INF("Certificate match\n");
      return 0;
    }

    LOG_INF("Certificate mismatch\n");
    err = modem_key_mgmt_delete(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
    if (err) {
      LOG_ERR("Failed to delete existing certificate, err %d\n", err);
    }
  }

  LOG_INF("Provisioning certificate");

  /*  Provision certificate to the modem */
  err = modem_key_mgmt_write(
      TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, ca_cert,
      sizeof(ca_cert) - 1
  );
  if (err) {
    LOG_ERR("Failed to provision certificate, err %d\n", err);
    return err;
  }
#else  /* CONFIG_MODEM_KEY_MGMT */
  err = tls_credential_add(
      TLS_SEC_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_cert, sizeof(ca_cert)
  );
  if (err == -EEXIST) {
    LOG_INF("CA certificate already exists, sec tag: %d\n", TLS_SEC_TAG);
  } else if (err < 0) {
    LOG_ERR("Failed to register CA certificate: %d\n", err);
    return err;
  }
#endif /* !CONFIG_MODEM_KEY_MGMT */

  return 0;
}

static void on_net_event_l4_disconnected(void) {
  LOG_INF("Disconnected from the network\n");
}

static void on_net_event_l4_connected(void) {
  k_sem_give(&network_connected_sem);
}

static void l4_event_handler(
    struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface
) {
  switch (event) {
    case NET_EVENT_L4_CONNECTED:
      LOG_INF("Network connectivity established and IP address assigned\n");
      on_net_event_l4_connected();
      break;
    case NET_EVENT_L4_DISCONNECTED:
      LOG_WRN("Network connectivity lost\n");
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
    LOG_ERR("Fatal error received from the connectivity layer\n");
    return;
  }
}

static void print_cellular_info(void) {
  int rc;
  int16_t rssi;
  char buffer[64];

  rc = cellular_get_signal(modem, CELLULAR_SIGNAL_RSSI, &rssi);
  if (!rc) {
    printk("RSSI %d\n", rssi);
  }

  rc = cellular_get_modem_info(
      modem, CELLULAR_MODEM_INFO_IMEI, &buffer[0], sizeof(buffer)
  );
  if (!rc) {
    printk("IMEI: %s\n", buffer);
  }
  rc = cellular_get_modem_info(
      modem, CELLULAR_MODEM_INFO_MODEL_ID, &buffer[0], sizeof(buffer)
  );
  if (!rc) {
    printk("MODEL_ID: %s\n", buffer);
  }
  rc = cellular_get_modem_info(
      modem, CELLULAR_MODEM_INFO_MANUFACTURER, &buffer[0], sizeof(buffer)
  );
  if (!rc) {
    printk("MANUFACTURER: %s\n", buffer);
  }
  rc = cellular_get_modem_info(
      modem, CELLULAR_MODEM_INFO_SIM_IMSI, &buffer[0], sizeof(buffer)
  );
  if (!rc) {
    printk("SIM_IMSI: %s\n", buffer);
  }
  rc = cellular_get_modem_info(
      modem, CELLULAR_MODEM_INFO_SIM_ICCID, &buffer[0], sizeof(buffer)
  );
  if (!rc) {
    printk("SIM_ICCID: %s\n", buffer);
  }
  rc = cellular_get_modem_info(
      modem, CELLULAR_MODEM_INFO_FW_VERSION, &buffer[0], sizeof(buffer)
  );
  if (!rc) {
    printk("FW_VERSION: %s\n", buffer);
  }
}

int lte_connect(void) {
  int err;

  /* Setup handler for Zephyr NET Connection Manager events. */
  net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
  net_mgmt_add_event_callback(&l4_cb);

  /* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
  net_mgmt_init_event_callback(
      &conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK
  );
  net_mgmt_add_event_callback(&conn_cb);

#if CONFIG_NRF_MODEM_LIB
  err = nrf_modem_lib_init();
  if (err) {
    LOG_ERR("Failed to initialize modem library!");
    return err;
  }
#else
  LOG_INF("Powering up modem");
  pm_device_action_run(modem, PM_DEVICE_ACTION_RESUME);
#endif

  /* Provision certificates before connecting to the network */
  err = cert_provision();
  if (err) {
    return 0;
  }

  LOG_INF("Bringing network interface up\n");

  /* Connecting to the configured connectivity layer.
   * Wi-Fi or LTE depending on the board that the sample was built for.
   */
  err = conn_mgr_all_if_up(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_up, error: %d\n", err);
    return err;
  }

  LOG_INF("Connecting to the network\n");

  err = conn_mgr_all_if_connect(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_connect, error: %d\n", err);
    return 0;
  }

  k_sem_take(&network_connected_sem, K_FOREVER);

  print_cellular_info();

  return 0;
}

int lte_disconnect(void) {
  int err;

  /* A small delay for the TCP connection teardown */
  k_sleep(K_SECONDS(1));

  /* The HTTP transaction is done, take the network connection down */
  err = conn_mgr_all_if_disconnect(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_disconnect, error: %d\n", err);
  }

  err = conn_mgr_all_if_down(true);
  if (err) {
    LOG_ERR("conn_mgr_all_if_down, error: %d\n", err);
  }

  return err;
}
