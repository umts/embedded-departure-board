/** @headerfile lte_manager.h */
#include "lte_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_MODEM_KEY_MGMT
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <modem/nrf_modem_lib.h>
#endif

#ifdef CONFIG_NET_SOCKETS_SOCKOPT_TLS
#include <zephyr/net/net_if.h>
#include <zephyr/net/tls_credentials.h>
#endif

LOG_MODULE_REGISTER(lte_manager, LOG_LEVEL_DBG);

static const char ca_cert[] = {
#include "../keys/public/jes-contact.pem"
};

#if CONFIG_MODEM_KEY_MGMT
BUILD_ASSERT(sizeof(ca_cert) < KB(4), "Certificates too large");
#endif

K_SEM_DEFINE(lte_connected_sem, 1, 1);

#ifdef CONFIG_NRF_MODEM_LIB_ON_FAULT_APPLICATION_SPECIFIC
void nrf_modem_fault_handler(struct nrf_modem_fault_info *fault_info) {
  LOG_ERR("Reason: %d", fault_info->reason);
  LOG_ERR("Program Counter: %d", fault_info->program_counter);
  LOG_ERR("%s", nrf_modem_lib_fault_strerror(errno));
}
#endif

/* Provision certificate to modem */
int provision_cert(nrf_sec_tag_t sec_tag, const char cert[], size_t cert_len) {
  int err;

#ifdef CONFIG_MODEM_KEY_MGMT
  bool exists;
  int mismatch;

  err = modem_key_mgmt_exists(
      sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists
  );
  if (err) {
    LOG_ERR("Failed to check for certificates err %d\n", err);
    return err;
  }

  if (exists) {
    mismatch = modem_key_mgmt_cmp(
        sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert, cert_len
    );
    if (!mismatch) {
      LOG_INF("Certificate match\n");
      return 0;
    }

    LOG_INF("Certificate mismatch\n");
    err = modem_key_mgmt_delete(sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
    if (err) {
      LOG_ERR("Failed to delete existing certificate, err %d\n", err);
    }
  }

  LOG_INF("Provisioning certificate");

  /*  Provision certificate to the modem */
  err = modem_key_mgmt_write(
      sec_tag, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert, cert_len
  );
  if (err) {
    LOG_ERR("Failed to provision certificate, err %d\n", err);
    return err;
  }
#else  /* CONFIG_MODEM_KEY_MGMT */
  err = tls_credential_add(
      sec_tag, TLS_CREDENTIAL_CA_CERTIFICATE, ca_cert, sizeof(ca_cert)
  );
  if (err == -EEXIST) {
    LOG_INF("CA certificate already exists, sec tag: %d\n", sec_tag);
  } else if (err < 0) {
    LOG_ERR("Failed to register CA certificate: %d\n", err);
    return err;
  }
#endif /* !CONFIG_MODEM_KEY_MGMT */

  return 0;
}

int lte_connect(void) {
  int err;
  k_sem_take(&lte_connected_sem, K_FOREVER);

#if CONFIG_NRF_MODEM_LIB
  err = nrf_modem_lib_init();
  if (err) {
    LOG_ERR("Failed to initialize modem library!");
    return err;
  }
#endif

  /* Provision certificates before connecting to the network */
  err = provision_cert(JES_SEC_TAG, ca_cert, sizeof(ca_cert));
  if (err) {
    LOG_ERR(
        "Failed to provision TLS certificate. TLS_SEC_TAG: %d", JES_SEC_TAG
    );
    return err;
  }

  LOG_INF("Initializing LTE interface");

  err = lte_lc_init();
  if (err < -1) {
    LOG_ERR("LTE failed to init. Err: %d", err);
    return err;
  }

  LOG_INF("Connecting to the network");

  err = lte_lc_connect();
  if (err < -1) {
    LOG_ERR("LTE failed to connect. Err: %d", err);
    return err;
  }

  k_sem_give(&lte_connected_sem);

  return 0;
}

int lte_disconnect(void) {
  int err;

  /* A small delay for the TCP connection teardown */
  k_sleep(K_SECONDS(1));

  err = lte_lc_deinit();
  if (err < -1) {
    LOG_ERR("Failed to deinit lte lib. Err: %d", err);
    return err;
  }

  err = nrf_modem_lib_shutdown();
  if (err) {
    LOG_ERR("Failed to shutdown nrf modem. Err: %d", err);
    return err;
  }

  return err;
}
