#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/net/socket.h>

/* modem includes */
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <modem/nrf_modem_lib.h>

int modem_init_and_connect_to_network(void) {
  int err = nrf_modem_lib_init();
  if (err) {
    // LOG_ERR("Failed to initialize modem library!");
    return err;
  }

  err = lte_lc_init_and_connect();
  if (err < -1) {
    // LOG_ERR("LTE failed to connect. Err: %d", err);
    return err;
  }

  return 0;
}
