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
#endif

#ifdef CONFIG_MODEM_HL7800
#include <zephyr/drivers/modem/hl7800.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/tls_credentials.h>

// static struct mdm_hl7800_apn *lte_apn_config;
// static struct mdm_hl7800_callback_agent hl7800_evt_agent;
#endif

LOG_MODULE_REGISTER(connection_manager, LOG_LEVEL_DBG);

#define TLS_SEC_TAG 42

static const char ca_cert[] = {
#include "../keys/public/jes-contact.pem"
};

#if CONFIG_MODEM_KEY_MGMT
BUILD_ASSERT(sizeof(ca_cert) < KB(4), "Certificate too large");
#endif

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
  int rsrp, sinr;
  char *imei, *imsi, *iccid, *fw_version, *sn;
  int32_t op_index, func;

  (void)mdm_hl7800_get_signal_quality(&rsrp, &sinr);
  LOG_INF("RSRP %d", rsrp);
  LOG_INF("SINR %d", sinr);

  imei = mdm_hl7800_get_imei();
  LOG_INF("IMEI: %s\n", imei);

  sn = mdm_hl7800_get_sn();
  LOG_INF("SERIAL_NUMBER: %s\n", sn);

  imsi = mdm_hl7800_get_imsi();
  LOG_INF("SIM_IMSI: %s\n", imsi);

  iccid = mdm_hl7800_get_iccid();
  LOG_INF("SIM_ICCID: %s\n", iccid);

  fw_version = mdm_hl7800_get_fw_version();
  LOG_INF("FW_VERSION: %s\n", fw_version);

  op_index = mdm_hl7800_get_operator_index();
  if (op_index < 0) {
    LOG_ERR("Operator Index Err: %d\n", op_index);
  }

  func = mdm_hl7800_get_functionality();
  if (op_index < 0) {
    LOG_ERR("Modem Functionality Err: %d\n", op_index);
  } else {
    LOG_INF("Modem Functionality: %d\n", op_index);
  }
}

int lte_connect(void) {
  int err;
  struct net_if *const iface = net_if_get_first_by_type(&NET_L2_GET_NAME(PPP));
  // uint16_t *port;

  /* Setup handler for Zephyr NET Connection Manager events. */
  net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
  net_mgmt_add_event_callback(&l4_cb);

  /* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
  net_mgmt_init_event_callback(
      &conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK
  );
  net_mgmt_add_event_callback(&conn_cb);

  LOG_INF("Powering up modem");
#if CONFIG_NRF_MODEM_LIB
  err = nrf_modem_lib_init();
  if (err) {
    LOG_ERR("Failed to initialize modem library!");
    return err;
  }
#endif

#ifdef CONFIG_MODEM_HL7800
  mdm_hl7800_log_filter_set(4);
  err = mdm_hl7800_reset();
  if (err != 0) {
    LOG_ERR("Error starting modem, Error: %d", err);
    return err;
  }
#endif

  /* Provision certificates before connecting to the network */
  err = cert_provision();
  if (err) {
    return 0;
  }

  LOG_INF("Bringing network interface up");

  /* Connecting to the configured connectivity layer.
   * Wi-Fi or LTE depending on the board that the sample was built for.
   */
  // err = conn_mgr_all_if_up(true);
  // if (err) {
  //   LOG_ERR("conn_mgr_all_if_up, error: %d\n", err);
  //   return err;
  // }

  // LOG_INF("Connecting to the network\n");

  // err = conn_mgr_all_if_connect(true);
  // if (err) {
  //   LOG_ERR("conn_mgr_all_if_connect, error: %d\n", err);
  //   return 0;
  // }
  // k_msleep(20000);

  err = net_if_up(iface);
  if (err < 0) {
    LOG_ERR("Failed to bring up network interface");
    // return -1;
  }

  // k_sem_take(&network_connected_sem, K_FOREVER);
  k_msleep(10000);

  print_cellular_info();
  LOG_INF("Modem connected!");

  return 0;
}

int lte_disconnect(void) {
  int err;

  /* A small delay for the TCP connection teardown */
  k_sleep(K_SECONDS(1));

  LOG_INF("Power off modem");

#ifdef CONFIG_MODEM_HL7800
  err = mdm_hl7800_power_off();
  if (err != 0) {
    LOG_ERR("Err powering off modem off, Err: %d", err);
  }
#endif

  // /* The HTTP transaction is done, take the network connection
  // down */ err = conn_mgr_all_if_disconnect(true); if (err) {
  //   LOG_ERR("conn_mgr_all_if_disconnect, error: %d\n", err);
  // }

  // err = conn_mgr_all_if_down(true);
  // if (err) {
  //   LOG_ERR("conn_mgr_all_if_down, error: %d\n", err);
  // }

  return err;
}
