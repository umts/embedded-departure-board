/** @file fota.h
 *  @brief Macros and function defines for the HTTP client.
 */

#ifndef FOTA_H
#define FOTA_H

#include <zephyr/types.h>

int write_buffer_to_flash(char *data, size_t len, _Bool flush);

void image_validation(uint8_t area_id);

void image_info(uint8_t area_id);

void download_update(void);

#endif
