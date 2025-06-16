/*
 * DebugMonitorLog.c
 *
 *  Created on: 2024/08/21
 *      Author: M.Akino
 */

#include "fsl_debug_console.h"
#include "DebugMonitor.h"
#include "DebugMonitorLog.h"
#include "../mylib/circure.h"

#define BUFFERSIZE (128)

static uint8_t rxbuf[BUFFERSIZE] = {0};
static circure_t rxccr = {0,0,BUFFERSIZE,rxbuf};

void DebugMonitor_putsLog(uint8_t *str, uint8_t n)
{
	while (n--) {
		PUTCHAR(*str++);	// blocking !
	}

	return;
}

int DebugMonitor_getcLog(void)
{
	return GETCHAR();	// blocking !
}

void DebugMonitor_entryLog(uint8_t c)
{
	circure_put(&rxccr, c);

	return;
}

void DebugMonitor_idleLog(void)
{
	int c;

	while ((c = circure_get(&rxccr)) >= 0)
	{
		DebugMonitor_entry(eDebugMonitorInterface_Log, c, 1);
	}

	return;
}
