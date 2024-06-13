
#include "fota.h"

#include <string.h>
#include <sys/errno.h>
#include <zephyr/device.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/stats/stats.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

#include "custom_http_client.h"
#include "pm_config.h"

LOG_MODULE_REGISTER(fota, LOG_LEVEL_DBG);

#define STRINGIZE(arg) #arg
#define STRINGIZE_VALUE(arg) STRINGIZE(arg)
#define PM_MCUBOOT_PRIMARY_STRING STRINGIZE_VALUE(PM_MCUBOOT_PRIMARY_NAME)
#define PM_MCUBOOT_SECONDARY_STRING STRINGIZE_VALUE(PM_MCUBOOT_SECONDARY_NAME)

#define CONTENT_LENGTH_HEADER ""
#define SHA256_HEADER "sha-256: "

BUILD_ASSERT(
    FIXED_PARTITION_EXISTS(PM_MCUBOOT_SECONDARY_NAME),
    "Missing " PM_MCUBOOT_SECONDARY_STRING
    " fixed partition. Secondary slot partition is required!"
);

struct flash_img_context ctx;

/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

void image_validation(uint8_t area_id) {
  int rc;
  char buf[BOOT_IMG_VER_STRLEN_MAX];
  struct mcuboot_img_header header;

  boot_read_bank_header(area_id, &header, sizeof(header));
  snprintk(
      buf, sizeof(buf), "%d.%d.%d-%d", header.h.v1.sem_ver.major,
      header.h.v1.sem_ver.minor, header.h.v1.sem_ver.revision,
      header.h.v1.sem_ver.build_num
  );
  LOG_INF("Booting image: build time: " __DATE__ " " __TIME__);
  LOG_INF("Image Version: %s", buf);
  LOG_INF("Image size: %d", header.h.v1.image_size);
  rc = boot_is_img_confirmed();
  LOG_INF("Image is%s confirmed", rc ? "" : " not");
  if (!rc) {
    if (boot_write_img_confirmed()) {
      LOG_ERR("Failed to confirm image");
    } else {
      LOG_INF("Marked image as OK");
    }
  }
}

void image_info(uint8_t area_id) {
  int rc;
  char buf[BOOT_IMG_VER_STRLEN_MAX];
  struct mcuboot_img_header header;

  boot_read_bank_header(area_id, &header, sizeof(header));
  snprintk(
      buf, sizeof(buf), "%d.%d.%d-%d", header.h.v1.sem_ver.major,
      header.h.v1.sem_ver.minor, header.h.v1.sem_ver.revision,
      header.h.v1.sem_ver.build_num
  );
  LOG_INF("MCUboot swap type: %d", mcuboot_swap_type());
  LOG_INF("Image Version %s", buf);
  rc = boot_is_img_confirmed();
  LOG_INF("Image is%s confirmed OK", rc ? "" : " not");
}

int write_buffer_to_flash(char *data, size_t len, _Bool flush) {
  int rc;
  if (flush) {
    rc = flash_img_buffered_write(&ctx, data, len, true);
  } else {
    rc = flash_img_buffered_write(&ctx, data, len, false);
  }

  LOG_DBG("Flash img bytes written: %d", flash_img_bytes_written(&ctx));

  return rc;
}

void download_update(void) {
  int rc;

  char headers_buf[1024];
  char write_buf[CONFIG_IMG_BLOCK_BUF_SIZE];
  char *sha256_ptr;
  uint8_t sha256[32];

  rc = boot_erase_img_bank(PM_MCUBOOT_SECONDARY_ID);
  if (rc < 0) {
    LOG_ERR("Failed to erase secondary image bank");
  }

  rc = flash_img_init_id(&ctx, PM_MCUBOOT_SECONDARY_ID);
  if (rc < 0) {
    LOG_ERR("Failed to init stream flash");
  }

  (void)http_get_firmware(
      write_buf, sizeof(write_buf), headers_buf, sizeof(headers_buf)
  );

  LOG_DBG("mcuboot_swap_type: %d", mcuboot_swap_type());

  sha256_ptr = strstr(headers_buf, SHA256_HEADER);
  if (sha256_ptr == NULL) {
    LOG_WRN("sha-256 not found in headers");

    rc = boot_request_upgrade(BOOT_UPGRADE_TEST);
    if (rc < 0) {
      LOG_ERR("Failed to REQUEST FIRMWARE UPGRADE");
    }
  } else {
    sha256_ptr += (sizeof(SHA256_HEADER) - 1);

    // for (int i = 0; i < 64; i++) {
    //   if (char2hex(*(sha256_ptr + i), &sha256[i]) < 0) {
    //     LOG_ERR("char2hex failed");
    //   }
    // }
    // strncpy(sha256, sha256_ptr, 64);
    // sha256[64] = '\0';

    rc = hex2bin(sha256_ptr, 64, sha256, 32);
    if (rc != 32) {
      LOG_ERR("hex2bin failed: %d", rc);
    }
    // LOG_DBG(
    //     "fic = {.match = %s, .clen = %d}", sha256,
    //     flash_img_bytes_written(&ctx)
    // );

    struct flash_img_check fic = {
        .match = sha256, .clen = flash_img_bytes_written(&ctx)
    };

    rc = flash_img_check(&ctx, &fic, PM_MCUBOOT_SECONDARY_ID);
    if (rc < 0) {
      LOG_ERR("flash_img_check failed: %s (%d)", strerror(rc), rc);
      return;
    }

    LOG_DBG("Image check sucessful!");

    rc = boot_request_upgrade(BOOT_UPGRADE_PERMANENT);
    if (rc < 0) {
      LOG_ERR("Failed to REQUEST FIRMWARE UPGRADE");
    }
  }

  LOG_DBG("\nImage info: " PM_MCUBOOT_PRIMARY_STRING);
  (void)image_info(PM_MCUBOOT_PRIMARY_ID);

  LOG_DBG("\nImage info: " PM_MCUBOOT_SECONDARY_STRING);
  (void)image_info(PM_MCUBOOT_SECONDARY_ID);
}
