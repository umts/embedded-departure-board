/** @file connection_manager.h
 *  @brief Macros and function defines for the zephyr connection manager.
 */
#ifndef LTE_MANAGER_H
#define LTE_MANAGER_H

extern struct k_sem lte_connected_sem;
enum tls_sec_tags { JES_SEC_TAG = 1, GITHUB_SEC_TAG = 2 };

/** @fn int lte_connect(void)
 *  @brief Makes an HTTP GET request and returns a char pointer to the HTTP
 * response body buffer.
 */
int lte_connect(void);

/** @fn int lte_disconnect(void)
 *  @brief Makes an HTTP GET request and returns a char pointer to the HTTP
 * response body buffer.
 */
int lte_disconnect(void);
#endif
