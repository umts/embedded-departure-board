/** @file connection_manager.h
 *  @brief Macros and function defines for the zephyr connection manager.
 */
#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

extern struct k_sem network_connected_sem;

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
