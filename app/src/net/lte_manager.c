/** @headerfile lte_manager.h */
#include "lte_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_MODEM_KEY_MGMT
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <modem/nrf_modem_lib.h>
#else
#include <zephyr/net/tls_credentials.h>
#endif

#include "watchdog_app.h"

LOG_MODULE_REGISTER(lte_manager);

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

K_SEM_DEFINE(lte_connected_sem, 1, 1);

#ifdef CONFIG_NRF_MODEM_LIB_ON_FAULT_APPLICATION_SPECIFIC
void nrf_modem_fault_handler(struct nrf_modem_fault_info *fault_info) {
  LOG_ERR("Reason: %d", fault_info->reason);
  LOG_ERR("Program Counter: %d", fault_info->program_counter);
  LOG_ERR("%s", nrf_modem_lib_fault_strerror(errno));
}
#endif

#if defined(CONFIG_JES_FOTA) || defined(CONFIG_STOP_REQUEST_JES)
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
#endif  // CONFIG_JES_FOTA || CONFIG_STOP_REQUEST_JES

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
#if defined(CONFIG_JES_FOTA) || defined(CONFIG_STOP_REQUEST_JES)
  err = provision_cert(JES_SEC_TAG, jes_cert, sizeof(jes_cert));
  if (err) {
    LOG_ERR("Failed to provision TLS certificate. TLS_SEC_TAG: %d", JES_SEC_TAG);
    return err;
  }
#endif  // CONFIG_JES_FOTA || CONFIG_STOP_REQUEST_JES

  LOG_INF("Initializing LTE interface");
  err = wdt_feed(wdt, wdt_channel_id);
  if (err) {
    LOG_ERR("Failed to feed watchdog. Err: %d", err);
    return 1;
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

  err = lte_lc_power_off();
  if (err) {
    LOG_ERR("Failed to shutdown nrf modem. Err: %d", err);
    return err;
  }

  return err;
}
