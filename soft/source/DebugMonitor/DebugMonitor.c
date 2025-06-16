/*
 * DebugMonitor.c
 *
 *  Created on: 2024/08/20
 *      Author: M.Akino
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "DebugMonitor.h"

#include "DebugMonitorLog.h"

#define INPUTBUFSIZE 128
#define OUTPUTBUFSIZE 128

#define MEMORYDUMP
#define DIRECTORY

/*
 * Debug Monitor Phase
 */
typedef enum {
	ePhase_Idle,
	ePhase_Wait1,	// 1st'@'
	ePhase_Wait2,	// 2nd'@'
	ePhase_Active,	// 3rd'@'
} ePhase;

/*
 * Debug Monitor Result Code
 */
typedef enum {
	eResult_OK = 0,
	eResult_NG,
} eResult;

/*
 * Debug Monitor in/out work
 */
typedef struct {
	ePhase	phase;
	uint8_t ipos;
	uint8_t opos;
	uint8_t iBuffer[INPUTBUFSIZE];
	uint8_t oBuffer[OUTPUTBUFSIZE];
} DebugMonitor_t;

/*
 * Debug Monitor context
 */
static DebugMonitor_t debugMonitor[eDebugMonitorInterfaceNumOf] = {0};

/*
 * Debug Monitor Interface Identify
 */
static uint8_t *enterMsg[] = {
	"Log"
};

/*
 * Debug Monitor Local Out Functions
 */
void dmflush_force(eDebugMonitorInterface d)
{
	if (d < eDebugMonitorInterfaceNumOf) {
		DebugMonitor_t *pD = &debugMonitor[d];

		if (pD->opos) {
			switch (d) {
			case eDebugMonitorInterface_Log:
				DebugMonitor_putsLog(pD->oBuffer, pD->opos);
				pD->opos = 0;
				break;

			default:
				break;
			}
		}
	}

	return;
}

void dmputc_force(eDebugMonitorInterface d, uint8_t c)
{
	if (d < eDebugMonitorInterfaceNumOf) {
		DebugMonitor_t *pD = &debugMonitor[d];

		switch (d) {
		case eDebugMonitorInterface_Log:
			if (c == '\n') {
				dmputc_force(d, '\r');
			}
			pD->oBuffer[pD->opos++] = c;
			if (pD->opos == OUTPUTBUFSIZE) {
				DebugMonitor_putsLog(pD->oBuffer, pD->opos);
				pD->opos = 0;
			}
			break;

		default:
			break;
		}
	}

	return;
}

void dmputs_force(eDebugMonitorInterface d, uint8_t *str)
{
	while (*str) {
		dmputc_force(d, *str++);
	}
	dmflush_force(d);

	return;
}

static void vdmprintf(eDebugMonitorInterface d, uint8_t *fmt, va_list va)
{
	uint8_t buf[128];

	vsnprintf(buf, sizeof(buf), fmt, va);
	dmputs_force(d, buf);

	return;
}

void dmprintf_force(eDebugMonitorInterface d, uint8_t *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vdmprintf(d, fmt, va);
	va_end(va);

	return;
}

/*
 * Debug Monitor Global Functions
 */
void dmprintf(eDebugMonitorInterface d, uint8_t *fmt, ...)
{
	if (d < eDebugMonitorInterfaceNumOf) {
		DebugMonitor_t *pD = &debugMonitor[d];

		if (pD->phase == 0) {
			va_list va;

			va_start(va, fmt);
			vdmprintf(d, fmt, va);
			va_end(va);
		}
	}

	return;
}

void dmputc(eDebugMonitorInterface d, uint8_t c)
{
	if (d < eDebugMonitorInterfaceNumOf) {
		DebugMonitor_t *pD = &debugMonitor[d];

		if (pD->phase == 0) {
			dmputc_force(d, c);
		}
	}

	return;
}

void dmputs(eDebugMonitorInterface d, uint8_t *str)
{
	if (d < eDebugMonitorInterfaceNumOf) {
		DebugMonitor_t *pD = &debugMonitor[d];

		if (pD->phase == 0) {
			dmputs_force(d, str);
		}
	}

	return;
}

void dmflush(eDebugMonitorInterface d)
{
	if (d < eDebugMonitorInterfaceNumOf) {
		DebugMonitor_t *pD = &debugMonitor[d];

		if (pD->phase == 0) {
			dmflush_force(d);
		}
	}

	return;
}

#define dmprintf dmprintf_force
#define dmputc dmputc_force
#define dmputs dmputs_force
#define dmflush dmflush_force

#ifdef MEMORYDUMP

static void dmputh(eDebugMonitorInterface d, uint8_t n)
{
	dmputc(d, n < 10 ? n + '0' : n - 10 + 'a');

	return;
}

static void dmputb(eDebugMonitorInterface d, uint8_t n)
{
	dmputh(d, n >> 4);
	dmputh(d, n & 15);

	return;
}

static void dmputw(eDebugMonitorInterface d, uint16_t n)
{
	dmputb(d, n >> 8);
	dmputb(d, n & 255);

	return;
}

static void dmputl(eDebugMonitorInterface d, uint32_t n)
{
	dmputw(d, n >> 16);
	dmputw(d, n & 65535);

	return;
}

static void dmputsp(eDebugMonitorInterface d, uint8_t n)
{
	while (n--) dmputc(d, ' ');

	return;
}

static void MemoryDumpSubL(eDebugMonitorInterface d, uint32_t start, uint32_t end)
{
	uint32_t sta = start & ~15;
	//          01234567  01234567 01234567  01234567 01234567
	dmputs(d, "\n address  +3+2+1+0 +7+6+5+4  +b+a+9+8 +f+e+d+c");
	while (sta <= end) {
		uint32_t val = sta >= (start & ~3) ? *(uint32_t *)sta : 0;
		uint8_t buf[8];

		if ((sta & 15) == 0) {
			dmputc(d, '\n');
			dmputl(d, sta);
			dmputsp(d, 1);
		}
		else if ((sta & 7) == 0) {
			dmputsp(d, 1);
		}
		dmputsp(d, 1);
		for (int i = 0; i < 8; ++i) {
			buf[i] = (val & 15) < 10 ? (val & 15) + '0' : (val & 15) - 10 + 'a';
			val >>= 4;
		}
		if (sta < start) {
			uint8_t skip = start - sta;
			skip = skip > 4 ? 4 : skip;
			for (int i = 0; i < skip; ++i) {
				buf[i*2+0] = ' ';
				buf[i*2+1] = ' ';
			}
		}
		if ((sta + 3) > end) {
			for (int i = (end+1) & 3; i < 4; ++i) {
				buf[i*2+0] = ' ';
				buf[i*2+1] = ' ';
			}
		}
		for (int i = 8-1; i >= 0; --i) {
			dmputc(d, buf[i]);
		}

		sta += 4;
	}
	dmflush(d);

	return;
}

static void MemoryDumpSubW(eDebugMonitorInterface d, uint32_t start, uint32_t end)
{
	uint32_t sta = start & ~15;
	//          01234567  0123 0123 0123 0123  0123 0123 0123 0123
	dmputs(d, "\n address  +1+0 +3+2 +5+4 +7+6  +9+8 +b+a +d+c +f+e");
	while (sta <= end) {
		uint16_t val = sta >= (start & ~1) ? *(uint16_t *)sta : 0;
		uint8_t buf[4];

		if ((sta & 15) == 0) {
			dmputc(d, '\n');
			dmputl(d, sta);
			dmputsp(d, 1);
		}
		else if ((sta & 7) == 0) {
			dmputsp(d, 1);
		}
		dmputsp(d, 1);
		for (int i = 0; i < 4; ++i) {
			buf[i] = (val & 15) < 10 ? (val & 15) + '0' : (val & 15) - 10 + 'a';
			val >>= 4;
		}
		if (sta < start) {
			uint8_t skip = start - sta;
			skip = skip > 2 ? 2 : skip;
			for (int i = 0; i < skip; ++i) {
				buf[i*2+0] = ' ';
				buf[i*2+1] = ' ';
			}
		}
		if ((sta + 1) > end) {
			for (int i = (end+1) & 1; i < 2; ++i) {
				buf[i*2+0] = ' ';
				buf[i*2+1] = ' ';
			}
		}
		for (int i = 4-1; i >= 0; --i) {
			dmputc(d, buf[i]);
		}

		sta += 2;
	}
	dmflush(d);

	return;
}

static void MemoryDumpSubB(eDebugMonitorInterface d, uint32_t start, uint32_t end)
{
	uint32_t sta = start & ~15;
	//          01234567  01 01 01 01 01 01 01 01  01 01 01 01 01 01 01 01
	dmputs(d, "\n address  +0 +1 +2 +3 +4 +5 +6 +7  +8 +9 +a +b +c +d +e +f  0123456789abcdef");
	while (sta <= end) {
		uint8_t val = sta >= start ? *(uint8_t *)sta : 0;

		if ((sta & 15) == 0) {
			dmputc(d, '\n');
			dmputl(d, sta);
			dmputsp(d, 1);
		}
		else if ((sta & 7) == 0) {
			dmputsp(d, 1);
		}
		dmputsp(d, 1);
		if (sta < start) {
			dmputsp(d, 2);
		}
		else {
			dmputb(d, val);
		}

		sta++;
		if (((sta & 15) == 0) || (sta > end)) {
			while (sta & 15) {
				dmputsp(d, (sta & 7) == 0 ? 4 : 3);
				sta++;
			}

			uint32_t a = sta-16;
			uint8_t *p = (uint8_t *)a;

			dmputsp(d, 2);
			for (int i = 0; i < 16; ++i) {
				uint8_t c = *p++;
				if ((a >= start) && (a <= end) && (c >= ' ') && (c < 0x7f)) {
					dmputc(d, c);
				}
				else {
					dmputsp(d, 1);
				}
				a++;
			}
		}
		if (sta == 0) {
			break;
		}
	}
	dmflush(d);

	return;
}

static eResult MemoryDump(eDebugMonitorInterface d, uint8_t *cmd, uint8_t ofs)
{
	eResult result = eResult_OK;
	int help = 0;

	if (cmd[ofs]) {
		char *pw;
		uint32_t start = 0, end = 0;
		uint8_t len = 4;

		start = strtoul(&cmd[ofs], &pw, 0);
		dmprintf(d, " start=%08x, end=", start);
		while ((*pw == ' ') || (*pw == ',')) pw++;
		if ((*pw == 'S') || (*pw == 's')) {
			pw++;
			uint32_t size = strtoul(pw, &pw, 0);
			if (size) {
				end = start + (size - 1);
			}
			else {
				dmputc(d, '?');
				result = eResult_NG;
				help = 1;
			}
		}
		else if (*pw) {
			end = strtoul(pw, &pw, 0);
		}
		else {
			dmputc(d, '?');
			result = eResult_NG;
			help = 1;
		}
		if (result == eResult_OK) {
			dmprintf(d, "%08x, access=", end);
			while ((*pw == ' ') || (*pw == ',')) pw++;
			if (*pw) {
				if ((*pw == 'L') || (*pw == 'l')) {
					len = 4;
				}
				else if ((*pw == 'W') || (*pw == 'w')) {
					len = 2;
				}
				else if ((*pw == 'B') || (*pw == 'b')) {
					len = 1;
				}
				else {
					dmputc(d, '?');
					result = eResult_NG;
					help = 1;
				}
			}
			if (result == eResult_OK) {
				switch (len) {
				case 4:
					dmputs(d, "Long");
					break;
				case 2:
					dmputs(d, "Word");
					break;
				case 1:
					dmputs(d, "Byte");
					break;
				default:
					break;
				}
				if (start <= end) {
					switch (len) {
					case 4:
						MemoryDumpSubL(d, start, end);
						break;
					case 2:
						MemoryDumpSubW(d, start, end);
						break;
					case 1:
						MemoryDumpSubB(d, start, end);
						break;
					default:
						break;
					}
				}
				else {
					dmputs(d, " (start > end)");
					result = eResult_NG;
				}
			}
		}
	}
	else {
		help = 2;
	}
	if (help) {
		if (help == 1)
		{
			dmputc(d, '\n');
		}
		dmputs(d, " usage>MemoryDump start[nnnn], end[mmmm](, access[L:Long|W:Word|B:Byte])\n");
		dmputs(d, "                  start[nnnn], size[Smmmm](, access[L:Long|W:Word|B:Byte]))");
	}

	return result;
}

#define MEMORYDUMPCMD	{"MemoryDump start,[end|size](,[L|W|B])", MemoryDump},
#else	//#ifdef MEMORYDUMP
#define MEMORYDUMPCMD
#endif	//#ifdef MEMORYDUMP

#ifdef DIRECTORY

#include "ff.h"

static void DispDateTime(eDebugMonitorInterface d, uint16_t date, uint16_t time)
{
	dmprintf(d, "%04d/%02d/%02d  %02d:%02d:%02d  ",
			(date >> 9) + 1980,
			(date >> 5) & 0xf,
			(date >> 0) & 0x1f,
			(time >> 11),
			(time >> 5) & 0x3f,
			(time << 1) & 0x3f);

	return;
}

static void DispSize(eDebugMonitorInterface d, uint64_t size, int wide)
{
	int digit = 1;
	uint64_t chk = 10;

	while (1)
	{
		if (size < chk)
		{
			break;
		}
		if (++digit == 20)
		{
			break;
		}
		chk *= 10;
	}
	if (digit < 20)
	{
		chk /= 10;
	}
	wide -= digit + (digit - 1) / 3;
	while (wide-- > 0)
	{
		dmputc(d, ' ');
	}
	// 00,000,000,000,000,000,000
	while (digit > 0)
	{
		int num = size / chk;

		dmputc(d, num + '0');
		if (((--digit % 3) == 0) && digit)
		{
			dmputc(d, ',');
		}
		size %= chk;
		chk /= 10;
	}

	return;
}

static eResult DisplayDirectory(eDebugMonitorInterface d, uint8_t *path)
{
	eResult result = eResult_OK;
	DIR dir;
	FRESULT res;

	res = f_opendir(&dir, path);
	if (res == FR_OK)
	{
		int nfile = 0;
		int ndir = 0;
		uint64_t total = 0;

		dmprintf(d, " Directory of %s\n\n", path);
		while (1)
		{
			FILINFO fno;

			res = f_readdir(&dir, &fno);					/* Read a directory item */
			if (res != FR_OK || fno.fname[0] == 0) break;	/* Error or end of dir */
			DispDateTime(d, fno.fdate, fno.ftime);
            if (fno.fattrib & AM_DIR)
            {	/* Directory */
                dmprintf(d, "  <DIR>          %s\n", fno.fname);
                ndir++;
            }
            else
            {	/* File */
            	DispSize(d, fno.fsize, 16);
                dmprintf(d, " %s\n", fno.fname);
                total += fno.fsize;
                nfile++;
            }
 		}

		f_closedir(&dir);
		dmprintf(d, "%16d File(s) ", nfile);
		DispSize(d, total, 17);
		dmprintf(d, " bytes\n");
		dmprintf(d, "%16d Dir(s) ", ndir);
		{
			FATFS *fs;
			DWORD fre_clust;
			uint8_t dirpath[3];

			dirpath[0] = path[0];
			dirpath[1] = path[1];
			dirpath[2] = 0;
			res = f_getfree(dirpath, &fre_clust, &fs);
			if (res == FR_OK)
			{
				uint64_t free = fre_clust * fs->csize;

				free *= 512;
				DispSize(d, free, 18);
				dmprintf(d, " bytes");
			}
		}
	}
	else
	{
		dmprintf(d, " The system cannot find the path specified.");
		result = eResult_NG;
	}

	return result;
}

static eResult Directory(eDebugMonitorInterface d, uint8_t *cmd, uint8_t ofs)
{
	eResult result = eResult_OK;

	if ((cmd[ofs] == ' ') && (cmd[ofs+1] != 0))
	{
		result = DisplayDirectory(d, &cmd[ofs+1]);
	}
	else
	{
		dmputs(d, "usage>Directory drive:full-path\n");
		dmputs(d, "      Dir drive:full-path");
	}

	return result;
}

#define DIRECTORYCMD	{"Directory", Directory},
#define DIRCMD			{"Dir", Directory},
#else	//DIRECTORY
#define DIRECTORYCMD
#define DIRCMD
#endif	//DIRECTORY

static eResult Help(eDebugMonitorInterface d, uint8_t *cmd, uint8_t ofs);

#define HELPCMD	{"Help", Help},{"?", Help}

typedef struct {
	uint8_t *pCmdStr;
	eResult (*pCmdFnc)(eDebugMonitorInterface, uint8_t *, uint8_t);
} CommandList_t;

static CommandList_t commandList[] = {
	MEMORYDUMPCMD
	DIRECTORYCMD
	DIRCMD
	HELPCMD
};

static eResult Help(eDebugMonitorInterface d, uint8_t *cmd, uint8_t ofs)
{
	eResult result = eResult_OK;

	dmprintf(d, " --- Command List ---");
	for (int i = 0; i < sizeof(commandList)/sizeof(commandList[0]); ++i) {
		dmprintf(d, "\n %s", commandList[i].pCmdStr);
	}

	return result;
}

/*
 * Debug Monitor Command Execute
 */
static eResult commandExecute(eDebugMonitorInterface d, uint8_t *cmd)
{
	for (int i = 0; i < sizeof(commandList)/sizeof(commandList[0]); ++i) {
		uint8_t ofs = 0;
		uint8_t *pw = strchr(commandList[i].pCmdStr, ' ');
		if (pw) {
			ofs = pw - commandList[i].pCmdStr;
		}
		else {
			ofs = strlen(commandList[i].pCmdStr);
		}
		if (strncmp(commandList[i].pCmdStr, cmd, ofs) == 0) {
			if ((cmd[ofs] == '\0') || (cmd[ofs] == ' ')) {
				eResult result = (commandList[i].pCmdFnc)(d, cmd, ofs);
				if (result) {
					dmputc(d, '\n');
				}
				return result;
			}
		}
	}

	return eResult_NG;
}

/*
 * Debug Monitor Entry
 */
void DebugMonitor_entry(eDebugMonitorInterface d, uint8_t c, uint8_t echo)
{
	if (d < eDebugMonitorInterfaceNumOf) {
		DebugMonitor_t *pD = &debugMonitor[d];

		switch (pD->phase) {
		case ePhase_Idle:
			if (c == '@') {
				pD->phase++;	// ePhase_Wait1
			}
			break;

		case ePhase_Wait1:
			if (c == '@') {
				pD->phase++;	// ePhase_Wait2
			}
			else {
				pD->phase = ePhase_Idle;
			}
			break;

		case ePhase_Wait2:
			if (c == '@') {
				pD->phase++;	// ePhase_Active
				dmprintf(d, "\n[[[ Debug Monitor (%s) ]]]\n", enterMsg[d]);
				dmputc(d, '*');
				dmflush(d);
			}
			else {
				pD->phase = ePhase_Idle;
			}
			break;

		case ePhase_Active:
			if (c == '@') {
				dmputs(d, "\n[[[ Exit ]]]\n");
				pD->phase = ePhase_Idle;
			}
			else if ((c == '\r') || (c == '\n')) {
				c = c == '\n' ? '\r' : '\n';
				if (pD->ipos == 0) {
					dmputs(d, pD->iBuffer);
				}
				else if (echo == 0) {
					dmflush(d);
				}
				dmputc(d, c);
				if (commandExecute(d, pD->iBuffer)) {
					dmputs(d, " Error Occurred !");
				}
				dmputs(d, "\n*");
				pD->ipos = 0;
			}
			else if (c == '\b') {
				if (pD->ipos != 0) {
					pD->iBuffer[--pD->ipos] = 0;
					if (echo) {
						dmputc(d, c);
						dmputc(d, ' ');
						dmputc(d, c);
						dmflush(d);
					}
				}
			}
			else if (c >= ' ') {
				if (pD->ipos < (INPUTBUFSIZE-1)) {
					pD->iBuffer[pD->ipos++] = c;
					pD->iBuffer[pD->ipos] = 0;
					dmputc(d, c);
					if (echo) {
						dmflush(d);
					}
				}
			}
			break;

		default:
			break;
		}
	}

	return;
}

#undef dmprintf
#undef dmputc
#undef dmputs
#undef dmflush
