/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "usb_host_config.h"
#include "usb_host.h"
#include "fsl_device_registers.h"
#include "usb_host_msd.h"
#include "host_msd_fatfs.h"
#include "host_keyboard_mouse.h"
#include "host_keyboard.h"
#include "host_midi.h"
#include "fsl_common.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */
#include "app.h"
#include "DebugMonitor/DebugMonitor.h"

#if ((!USB_HOST_CONFIG_KHCI) && (!USB_HOST_CONFIG_EHCI) && (!USB_HOST_CONFIG_OHCI) && (!USB_HOST_CONFIG_IP3516HS))
#error Please enable USB_HOST_CONFIG_KHCI, USB_HOST_CONFIG_EHCI, USB_HOST_CONFIG_OHCI, or USB_HOST_CONFIG_IP3516HS in file usb_host_config.
#endif

#include "usb_phy.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief host callback function.
 *
 * device attach/detach callback function.
 *
 * @param deviceHandle        device handle.
 * @param configurationHandle attached device's configuration descriptor information.
 * @param eventCode           callback event code, please reference to enumeration host_event_t.
 *
 * @retval kStatus_USB_Success              The host is initialized successfully.
 * @retval kStatus_USB_NotSupported         The application don't support the configuration.
 */
static usb_status_t USB_HostEvent(usb_device_handle deviceHandle,
                                  usb_host_configuration_handle configurationHandle,
                                  uint32_t eventCode);

/*!
 * @brief app initialization.
 */
static void USB_HostApplicationInit(void);

static void USB_HostTask(void *param);

static void USB_HostApplicationTask(void *param);

extern void USB_HostClockInit(void);
extern void USB_HostIsrEnable(void);
extern void USB_HostTaskFn(void *param);
void BOARD_InitHardware(void);

void send_midi_test(char c);

/*******************************************************************************
 * Variables
 ******************************************************************************/
/*! @brief USB host msd fatfs instance global variable */
extern usb_host_msd_fatfs_instance_t g_MsdFatfsInstance;
/*! @brief USB host keyboard instance global variable */
extern usb_host_keyboard_instance_t g_HostHidKeyboard;
/*! @brief USB host midi instance global variable */
extern host_midi_instance_t g_HostMidi;
usb_host_handle g_HostHandle;
static TaskHandle_t g_HostAppHandle;
static TaskHandle_t g_DebugHandle;

/*******************************************************************************
 * Code
 ******************************************************************************/

void USB_OTG1_IRQHandler(void)
{
    USB_HostEhciIsrFunction(g_HostHandle);
}

void USB_HostClockInit(void)
{
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL,
        BOARD_USB_PHY_TXCAL45DP,
        BOARD_USB_PHY_TXCAL45DM,
    };

    CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
    CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
}

void USB_HostIsrEnable(void)
{
    uint8_t irqNumber;

    uint8_t usbHOSTEhciIrq[] = USBHS_IRQS;
    irqNumber                = usbHOSTEhciIrq[CONTROLLER_ID - kUSB_ControllerEhci0];
/* USB_HOST_CONFIG_EHCI */

/* Install isr, set priority, and enable IRQ. */
#if defined(__GIC_PRIO_BITS)
    GIC_SetPriority((IRQn_Type)irqNumber, USB_HOST_INTERRUPT_PRIORITY);
#else
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_HOST_INTERRUPT_PRIORITY);
#endif
    EnableIRQ((IRQn_Type)irqNumber);
}

void USB_HostTaskFn(void *param)
{
    USB_HostEhciTaskFunction(param);
}

/*!
 * @brief USB isr function.
 */

static usb_status_t USB_HostEvent(usb_device_handle deviceHandle,
                                  usb_host_configuration_handle configurationHandle,
                                  uint32_t eventCode)
{
    usb_status_t status1;
    usb_status_t status2;
    usb_status_t status3;
    usb_status_t status = kStatus_USB_Success;
    switch (eventCode & 0x0000FFFFU)
    {
        case kUSB_HostEventAttach:
            status1 = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
            status2 = USB_HostHidKeyboardEvent(deviceHandle, configurationHandle, eventCode);
            status3 = USB_HostMidiEvent(deviceHandle, configurationHandle, eventCode);
            if ((status1 == kStatus_USB_NotSupported) &&
            	(status2 == kStatus_USB_NotSupported) &&
				(status3 == kStatus_USB_NotSupported))
            {
                status = kStatus_USB_NotSupported;
            }
            break;

        case kUSB_HostEventNotSupported:
            usb_echo("device not supported.\r\n");
            break;

        case kUSB_HostEventEnumerationDone:
            status1 = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
            status2 = USB_HostHidKeyboardEvent(deviceHandle, configurationHandle, eventCode);
            status3 = USB_HostMidiEvent(deviceHandle, configurationHandle, eventCode);
            if ((status1 != kStatus_USB_Success) &&
            	(status2 != kStatus_USB_Success) &&
				(status3 != kStatus_USB_Success))
            {
                status = kStatus_USB_Error;
            }
            break;

        case kUSB_HostEventDetach:
            status1 = USB_HostMsdEvent(deviceHandle, configurationHandle, eventCode);
            status2 = USB_HostHidKeyboardEvent(deviceHandle, configurationHandle, eventCode);
            status3 = USB_HostMidiEvent(deviceHandle, configurationHandle, eventCode);
            if ((status1 != kStatus_USB_Success) &&
            	(status2 != kStatus_USB_Success) &&
				(status3 != kStatus_USB_Success))
            {
                status = kStatus_USB_Error;
            }
            break;

        case kUSB_HostEventEnumerationFail:
            usb_echo("enumeration failed\r\n");
            break;

        default:
            break;
    }
    return status;
}

static void USB_HostApplicationInit(void)
{
    usb_status_t status = kStatus_USB_Success;

    USB_HostClockInit();

#if ((defined FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT))
    SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

    status = USB_HostInit(CONTROLLER_ID, &g_HostHandle, USB_HostEvent);
    if (status != kStatus_USB_Success)
    {
        usb_echo("host init error\r\n");
        return;
    }
    USB_HostIsrEnable();

    usb_echo("host init done\r\n");
}

static void USB_HostTask(void *param)
{
    while (1)
    {
        USB_HostTaskFn(param);
    }
}

static void USB_HostApplicationTask(void *param)
{
    while (1)
    {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        USB_HostMsdTask(&g_MsdFatfsInstance);
        USB_HostHidKeyboardTask(&g_HostHidKeyboard);
        USB_HostMidiTask(&g_HostMidi);
    }
}

void USB_HostAppWakeUp(void)
{
	xTaskNotifyGive(g_HostAppHandle);
}

static void DebugMonitorTask(void *param)
{
	while (1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		DebugMonitor_idleLog();
	}
}

static void StdInTask(void *param)
{
	while (1)
	{
		int c = DebugMonitor_getcLog();

		DebugMonitor_entryLog(c);
		xTaskNotifyGive(g_DebugHandle);
		if (!dmcheck(eDebugMonitorInterface_Log))
		{
			send_midi_test(c);
		}
	}
}

void KeyboardToDebugMonitor(int c)
{
	c = c == 0x0a ? 0x0d : c;
	DebugMonitor_entryLog(c);
	xTaskNotifyGive(g_DebugHandle);
	if (!dmcheck(eDebugMonitorInterface_Log))
	{
		send_midi_test(c);
	}
}

#include "ctype.h"

void send_midi_test(char c)
{
	static const char key[] = "azsxcfvgbnjmk,l./";

	if (isupper(c))
	{
		c = tolower(c);
	}
	for (int i = 0; i < sizeof(key); i++)
	{
		if (c == key[i])
		{
			uint8_t note = 56 + i;

			USB_HostMidiSendShortMessage(0, 0x90 + 9, note, 100);
			USB_HostMidiSendShortMessage(0, 0x80 + 9, note, 64);
			break;
		}
	}
}

int main(void)
{
    BOARD_ConfigMPU();

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    USB_HostApplicationInit();

    if (xTaskCreate(USB_HostTask, "usb host task", 2000L / sizeof(portSTACK_TYPE), g_HostHandle, 4, NULL) != pdPASS)
    {
        usb_echo("create host task error\r\n");
    }
    if (xTaskCreate(USB_HostApplicationTask, "app task", 2300L / sizeof(portSTACK_TYPE), NULL, 3, &g_HostAppHandle) != pdPASS)
    {
        usb_echo("create app task error\r\n");
    }
    if (xTaskCreate(DebugMonitorTask, "debug monitor task", 2000L / sizeof(portSTACK_TYPE), NULL, 1, &g_DebugHandle) != pdPASS)
    {
    	usb_echo("create debug task error\r\n");
    }
    if (xTaskCreate(StdInTask, "stdin task", 2000L / sizeof(portSTACK_TYPE), NULL, 0, NULL) != pdPASS)
    {
    	usb_echo("create stdin task error\r\n");
    }

    vTaskStartScheduler();

    while (1)
    {
        ;
    }
}
