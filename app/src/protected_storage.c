#ifdef CONFIG_STOP_REQUEST_SWIFTLY
#include "protected_storage.h"

#include <psa/protected_storage.h>
#include <psa/storage_common.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(protected_storage);

#define SWIFTLY_KEY_PSA_UID 1

static const char swiftly_api_key[] = {
#include "../keys/private/swiftly-authorization.key"
};

int write_swiftly_api_key(void) {
  psa_status_t status = 0;

  LOG_INF(
      "PSA Protected Storage API Version %d.%d", PSA_PS_API_VERSION_MAJOR, PSA_PS_API_VERSION_MINOR
  );

  LOG_INF("Writing data to UID: %s", swiftly_api_key);
  psa_storage_uid_t uid = SWIFTLY_KEY_PSA_UID;
  psa_storage_create_flags_t uid_flag = PSA_STORAGE_FLAG_WRITE_ONCE;

  status = psa_ps_set(uid, sizeof(swiftly_api_key), swiftly_api_key, uid_flag);
  if (status != PSA_SUCCESS) {
    LOG_INF("Failed to store data! (%d)", status);
    return status;
  }

  /* Get info on UID */
  struct psa_storage_info_t uid_info;

  status = psa_ps_get_info(uid, &uid_info);
  if (status != PSA_SUCCESS) {
    LOG_INF("Failed to get info! (%d)", status);
    return status;
  }
  LOG_INF("Info on data stored in UID:");
  LOG_INF("- Size: %d", uid_info.size);
  LOG_INF("- Capacity: 0x%2x", uid_info.capacity);
  LOG_INF("- Flags: 0x%2x", uid_info.flags);

  LOG_INF("Read and compare data stored in UID");
  size_t bytes_read;
  char stored_data[sizeof(swiftly_api_key)];

  status = psa_ps_get(uid, 0, sizeof(swiftly_api_key), &stored_data, &bytes_read);
  if (status != PSA_SUCCESS) {
    LOG_INF("Failed to get data stored in UID! (%d)", status);
    return status;
  }
  LOG_INF("Data stored in UID: %s", stored_data);

  return 0;
}
#endif  // CONFIG_STOP_REQUEST_SWIFTLY
