#ifndef FOTA_H
#define FOTA_H

#ifdef CONFIG_JES_FOTA
#include <zephyr/types.h>

int write_buffer_to_flash(char *data, size_t len, _Bool flush);
void download_update(void);
#endif  // CONFIG_JES_FOTA

void validate_image(void);
#endif  // FOTA_H
