/*
 * DebugMonitorLog.h
 *
 *  Created on: 2024/08/21
 *      Author: M.Akino
 */

#ifndef DEBUGMONITORLOG_H_
#define DEBUGMONITORLOG_H_

#include <stdint.h>

void DebugMonitor_putsLog(uint8_t *str, uint8_t n);	// blocking
int DebugMonitor_getcLog(void);	// blocking
void DebugMonitor_entryLog(uint8_t c);	// blocking
void DebugMonitor_idleLog(void);	// blocking

#endif /* DEBUGMONITORLOG_H_ */
