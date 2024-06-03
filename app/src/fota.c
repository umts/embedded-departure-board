
#include "fota.h"

#include <custom_http_client.h>
#include <pm_config.h>
#include <sys/_stdint.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/stats/stats.h>

////

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
// #include <zephyr/storage/stream_flash.h>

LOG_MODULE_REGISTER(fota, LOG_LEVEL_DBG);

// stream_flash_init()

BUILD_ASSERT(
    FIXED_PARTITION_EXISTS(image_1), "Missing image_1 fixed partition"
);

#define NVS_PARTITION image_1
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE FIXED_PARTITION_SIZE(NVS_PARTITION)
// #define EXTERNAL_FLASH DEVICE_DT_GET(DT_ALIAS(external_flash))

const struct device *const external_flash = NVS_PARTITION_DEVICE;

// struct stream_flash_ctx ctx;
// const char settings_key;
// stream_flash_callback_t flash_write_cb;

// static int flash_write_cb(
//     unsigned char *buf, unsigned int len, unsigned int offset
// ) {
//   LOG_DBG("It's a callback");
//   return 0;
// }

static size_t write_block_size;

int write_buffer_to_flash(char *data, size_t len, size_t offset) {
  if (len % write_block_size == 0) {
    // LOG_DBG("Writing to flash");
    if (flash_write(external_flash, offset, data, len)) {
      return -1;
    }
  } else {
    return len;
  }

  return 0;
}

void flash_debug(void) {
  struct flash_pages_info info;
  if (!device_is_ready(external_flash)) {
    LOG_WRN("external_flash isn't ready!");
  }

  LOG_DBG("write_block_size %d", flash_get_write_block_size(external_flash));

  flash_get_page_info_by_offs(external_flash, 0, &info);

  const struct flash_parameters *params = flash_get_parameters(external_flash);

  LOG_DBG(
      "size %d, start_offset: %ld, index: %d, write_block_size: %d, "
      "erase_value: %d, page_cout: %d",
      info.size, info.start_offset, info.index, params->write_block_size,
      params->erase_value, flash_get_page_count(external_flash)
  );

  // flash_erase(external_flash, 0, 0x0000d8000);
}

void download_update(void) {
  // int rc;
  // char buf[BOOT_IMG_VER_STRLEN_MAX];
  // struct mcuboot_img_header header;

  char write_buf[1280];

  if (!device_is_ready(external_flash)) {
    LOG_WRN("external_flash isn't ready!");
  }

  write_block_size = flash_get_write_block_size(external_flash);

  LOG_DBG("write_block_size %d", write_block_size);

  // rc = stream_flash_init(
  //     &ctx, external_flash, write_buf, sizeof(write_buf),
  //     NVS_PARTITION_OFFSET, NVS_PARTITION_SIZE, NULL
  // );
  // if (rc < 0) {
  //   LOG_ERR("Failed to init stream flash");
  // }

  // stream_flash_erase_page(&ctx, 0);

  // LOG_DBG("Initialized stream flash");

  // while (stream_flash_progress_load(&ctx, &settings_key) > 0)
  //   ;

  (void)http_get_firmware(write_buf, sizeof(write_buf));

  // stream_flash_progress_clear(&ctx, &settings_key);

  // rc = boot_request_upgrade(BOOT_UPGRADE_TEST);
}

/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

void image_validation(void) {
  int rc;
  char buf[BOOT_IMG_VER_STRLEN_MAX];
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
