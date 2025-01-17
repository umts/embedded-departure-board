/** @file connection_manager.h
 *  @brief Macros and function defines for the zephyr connection manager.
 */
#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

extern struct k_sem network_connection_sem;
enum tls_sec_tags { NO_SEC_TAG, JES_SEC_TAG, BUSTRACKER_SEC_TAG };

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
