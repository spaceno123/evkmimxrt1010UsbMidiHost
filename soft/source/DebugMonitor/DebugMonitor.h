/*
 * DebugMonitor.h
 *
 *  Created on: 2024/08/20
 *      Author: M.Akino
 */

#ifndef DEBUGMONITOR_H_
#define DEBUGMONITOR_H_

#include <stdint.h>

typedef enum {
	eDebugMonitorInterface_Log = 0,

	eDebugMonitorInterfaceNumOf,
} eDebugMonitorInterface;

void dmprintf(eDebugMonitorInterface d, uint8_t *fmt, ...);	// with dflush()
void dmputc(eDebugMonitorInterface d, uint8_t c);			// without dflush()
void dmputs(eDebugMonitorInterface d, uint8_t *str);			// with dflush()
void dmflush(eDebugMonitorInterface d);
int dmcheck(eDebugMonitorInterface d);		// 1:monitor active, 0:monitor un-active

void DebugMonitor_entry(eDebugMonitorInterface d, uint8_t c, uint8_t echo);

#include "DebugMonitorLog.h"

#endif /* DEBUGMONITOR_H_ */
