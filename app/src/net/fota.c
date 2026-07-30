#ifdef CONFIG_BOOTLOADER_MCUBOOT

#include "fota.h"

#include <zephyr/device.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(fota);

#define STRINGIZE(arg) #arg
#define STRINGIZE_VALUE(arg) STRINGIZE(arg)
#define MCUBOOT_SECONDARY_LABEL slot1_partition
#define MCUBOOT_SECONDARY_STRING STRINGIZE_VALUE(MCUBOOT_SECONDARY_LABEL)

#define CONTENT_LENGTH_HEADER ""
#define SHA256_HEADER "sha-256: "

BUILD_ASSERT(
    FIXED_PARTITION_EXISTS(MCUBOOT_SECONDARY_LABEL),
    "Missing " MCUBOOT_SECONDARY_STRING
    " fixed partition. Secondary slot partition is required!"
);

void validate_image(void) {
  int rc;
  char buf[BOOT_IMG_VER_STRLEN_MAX];
  struct mcuboot_img_header header;

  boot_read_bank_header(PARTITION_ID(slot0_partition), &header, sizeof(header));
  snprintk(
      buf, sizeof(buf), "%d.%d.%d-%d", header.h.v1.sem_ver.major,
      header.h.v1.sem_ver.minor, header.h.v1.sem_ver.revision,
      header.h.v1.sem_ver.build_num
  );
  LOG_INF("MCUboot swap type: %d", mcuboot_swap_type());
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
#endif  // CONFIG_BOOTLOADER_MCUBOOT
