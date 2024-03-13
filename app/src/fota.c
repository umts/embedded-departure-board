#include <custom_http_client.h>
#include <fota.h>
#include <pm_config.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/stats/stats.h>

LOG_MODULE_REGISTER(fota, CONFIG_LOG_DEFAULT_LEVEL);

void download_update(void) { (void)http_get_firmware(); }

/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

void image_validation(void) {
  int rc;
  char buf[255];
  struct mcuboot_img_header header;

  boot_read_bank_header(PM_MCUBOOT_PRIMARY_ID, &header, sizeof(header));
  snprintk(
      buf, sizeof(buf), "%d.%d.%d-%d", header.h.v1.sem_ver.major,
      header.h.v1.sem_ver.minor, header.h.v1.sem_ver.revision,
      header.h.v1.sem_ver.build_num
  );

  LOG_INF("Booting image: build time: " __DATE__ " " __TIME__);
  LOG_INF("Image Version %s", buf);
  rc = boot_is_img_confirmed();
  LOG_INF("Image is%s confirmed OK", rc ? "" : " not");
  if (!rc) {
    if (boot_write_img_confirmed()) {
      LOG_ERR("Failed to confirm image");
    } else {
      LOG_INF("Marked image as OK");
    }
  }
}
