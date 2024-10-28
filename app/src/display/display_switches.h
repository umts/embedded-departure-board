/** @file display_switches.h
 *  @brief LED display enable switch defines & functions
 */

#ifndef DISPLAY_SWITCHES_H
#define DISPLAY_SWITCHES_H

int init_display_switches(void);
int display_on(const int display_number);
int display_off(const int display_number);

#endif
