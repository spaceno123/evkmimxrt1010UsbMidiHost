/*
 * host_midi.c
 *
 *  Created on: 2025/06/18
 *      Author: M.Akino
 */

#include <stdio.h>
#include "usb_host_config.h"
#include "usb_host.h"
#include "usb_host_midi.h"
#include "host_midi.h"
#include "app.h"
#include "mylib/usbmidi.h"
#include "mylib/circure.h"
#include "DebugMonitor/DebugMonitor.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

void USB_HostAppWakeUp(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_midiRxBuffer[MIDI_BUFFER_SIZE]; /*!< use to receive data */
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_midiTxBuffer[MIDI_BUFFER_SIZE]; /*!< use to transfer data */

static uint32_t s_midiTxPacketBuffer[MIDI_BUFFER_SIZE/4*2];
static circure_t ccrtxpacket = {0,0,MIDI_BUFFER_SIZE/4*2,s_midiTxPacketBuffer};

host_midi_instance_t g_HostMidi;

/*******************************************************************************
 * Code
 ******************************************************************************/

bool USB_HostMidiSendShortMessage(uint8_t cn, uint8_t sts, uint8_t dt1, uint8_t dt2)
{
	bool ret;
	SUSBMIDI sUsbMidi;

	sUsbMidi.sPacket.CN_CIN = (cn << 4) | (sts >> 4);
	sUsbMidi.sPacket.MIDI_0 = sts;
	sUsbMidi.sPacket.MIDI_1 = dt1;
	sUsbMidi.sPacket.MIDI_2 = dt2;

	ret = circure_putl(&ccrtxpacket, sUsbMidi.ulData);
	USB_HostAppWakeUp();

	return ret;
}

static void USB_HostMidiProcessBuffer(host_midi_instance_t *midiInstance)
{
	int len = midiInstance->receiveCount;

	if (len)
	{
		uint32_t *p = (uint32_t *)midiInstance->midiRxBuffer;

//		dmprintf(eDebugMonitorInterface_Log, "\nlen=%d", len);
		len /= 4;
		while (len--)
		{
			SUSBMIDI sUsbMidi;

			sUsbMidi.ulData = *p++;
//			if (GetUsbMidiCn(sUsbMidi.sPacket.CN_CIN) == USBMIDI_OUT_CN) {
//				PacketToStream(&sPacMidi, sUsbMidi.ulData);
//			}
			if (sUsbMidi.ulData && (sUsbMidi.sPacket.MIDI_0 < 0xf0))
			{
				dmprintf(eDebugMonitorInterface_Log, "\n%02x:%02x:%02x:%02x",
						 sUsbMidi.sPacket.CN_CIN,
						 sUsbMidi.sPacket.MIDI_0,
						 sUsbMidi.sPacket.MIDI_1,
						 sUsbMidi.sPacket.MIDI_2);
			}
		}
	}
}

/*!
 * @brief host midi data transfer callback.
 *
 * This function is used as callback function for bulk in transfer .
 *
 * @param param    the host midi instance pointer.
 * @param data     data buffer pointer.
 * @param dataLength data length.
 * @status         transfer result status.
 */
static void USB_HostMidiInCallback(void *param, uint8_t *data, uint32_t dataLength, usb_status_t status)
{
    host_midi_instance_t *midiInstance = (host_midi_instance_t *)param;

    if (midiInstance->runWaitState == kUSB_HostMidiRunWaitDataReceived)
    {
        if (midiInstance->deviceState == kStatus_DEV_Attached)
        {
            if (status == kStatus_USB_Success)
            {
                midiInstance->runState = kUSB_HostMidiRunDataReceived; /* go to process data */
                midiInstance->receiveCount = dataLength;
            }
            else
            {
                midiInstance->runState = kUSB_HostMidiRunPrimeDataReceive; /* go to prime next receiving */
                midiInstance->receiveCount = 0;
            }
            USB_HostAppWakeUp();
        }
    }
}

/*!
 * @brief host midi data transfer callback.
 *
 * This function is used as callback function for bulk out transfer .
 *
 * @param param    the host midi instance pointer.
 * @param data     data buffer pointer.
 * @param dataLength data length.
 * @status         transfer result status.
 */
static void USB_HostMidiOutCallback(void *param, uint8_t *data, uint32_t dataLength, usb_status_t status)
{
    host_midi_instance_t *midiInstance = (host_midi_instance_t *)param;

    if (midiInstance->sendBusy)
    {
    	midiInstance->sendBusy = 0;
        USB_HostAppWakeUp();
    }
}

static void USB_HostMidiControlCallback(void *param, uint8_t *data, uint32_t dataLength, usb_status_t status)
{
    host_midi_instance_t *midiInstance = (host_midi_instance_t *)param;
    bool req = false;

    if (midiInstance->runWaitState == kUSB_HostMidiRunWaitSetInterface) /* set interface done */
    {
        midiInstance->runState = kUSB_HostMidiRunSetInterfaceDone;
        req = true;
    }
    else
    {
    }

    if (req)
    {
        USB_HostAppWakeUp();
    }
}

void USB_HostMidiTask(void *param)
{
    host_midi_instance_t *midiInstance = (host_midi_instance_t *)param;

    /* device state changes, process once for each state */
    if (midiInstance->deviceState != midiInstance->prevState)
    {
        midiInstance->prevState = midiInstance->deviceState;
        switch (midiInstance->deviceState)
        {
            case kStatus_DEV_Idle:
                break;

            case kStatus_DEV_Attached: /* deivce is attached and numeration is done */
                /* midi class initialization */
                if (USB_HostMidiInit(midiInstance->deviceHandle, &midiInstance->classHandle) !=
                    kStatus_USB_Success)
                {
                    usb_echo("host midi class initialize fail\r\n");
                    return;
                }
                else
                {
                    usb_echo("midi attached\r\n");
                }
                midiInstance->runState = kUSB_HostMidiRunSetInterface;
                break;

            case kStatus_DEV_Detached: /* device is detached */
                midiInstance->runState    = kUSB_HostMidiRunIdle;
                midiInstance->deviceState = kStatus_DEV_Idle;
                USB_HostMidiDeinit(midiInstance->deviceHandle,
                                   midiInstance->classHandle); /* midi class de-initialization */
                midiInstance->classHandle = NULL;
                usb_echo("midi detached\r\n");
                break;

            default:
                break;
        }
    }

    switch (midiInstance->runState)
    {
        case kUSB_HostMidiRunIdle:
        	if (midiInstance->attachFlag)
        	{	// now send enable
        		if (!midiInstance->sendBusy)
        		{
        			int count = circure_remain(&ccrtxpacket);

        			if (count)
        			{
            			int maxlen = midiInstance->bulkOutMaxPacketSize / 4;
            			uint32_t *p = (uint32_t *)midiInstance->midiTxBuffer;

            			if (count > maxlen)
            			{
            				count = maxlen;
            			}
            			for (int i = 0; i < count; i++)
            			{
            				*p++ = circure_getl(&ccrtxpacket);
            			}
            			midiInstance->sendBusy = 1;
                        USB_HostMidiSend(midiInstance->classHandle, midiInstance->midiTxBuffer,
                                         count * 4, USB_HostMidiOutCallback, midiInstance);
        			}
        		}
        	}
        	else
        	{	// now send disable
    			int count = circure_remain(&ccrtxpacket);

    			if (count)
    			{
    				circure_clear(&ccrtxpacket);
    			}
        	}
            break;

        case kUSB_HostMidiRunSetInterface: /* 1. set midi interface */
            midiInstance->runWaitState = kUSB_HostMidiRunWaitSetInterface;
            midiInstance->runState     = kUSB_HostMidiRunIdle;
            if (USB_HostMidiSetInterface(midiInstance->classHandle, midiInstance->interfaceHandle, 0,
                                         USB_HostMidiControlCallback, midiInstance) != kStatus_USB_Success)
            {
                usb_echo("set interface error\r\n");
            }
            break;

        case kUSB_HostMidiRunSetInterfaceDone: /* 2. start to receive data */
            midiInstance->bulkInMaxPacketSize =
                USB_HostMidiGetPacketsize(midiInstance->classHandle, USB_ENDPOINT_BULK, USB_IN);
            midiInstance->bulkOutMaxPacketSize =
                USB_HostMidiGetPacketsize(midiInstance->classHandle, USB_ENDPOINT_BULK, USB_OUT);
            midiInstance->attachFlag = 1;

            midiInstance->runWaitState = kUSB_HostMidiRunWaitDataReceived;
            midiInstance->runState     = kUSB_HostMidiRunIdle;
            if (USB_HostMidiRecv(midiInstance->classHandle, midiInstance->midiRxBuffer,
                                 midiInstance->bulkInMaxPacketSize, USB_HostMidiInCallback,
                                 midiInstance) != kStatus_USB_Success)
            {
                usb_echo("error in USB_HostMidiRecv\r\n");
            }
            break;

        case kUSB_HostMidiRunDataReceived: /* process received data and receive next data */
            USB_HostMidiProcessBuffer(midiInstance);

            midiInstance->runWaitState = kUSB_HostMidiRunWaitDataReceived;
            midiInstance->runState     = kUSB_HostMidiRunIdle;
            if (USB_HostMidiRecv(midiInstance->classHandle, midiInstance->midiRxBuffer,
                                 midiInstance->bulkInMaxPacketSize, USB_HostMidiInCallback,
                                 midiInstance) != kStatus_USB_Success)
            {
                usb_echo("Error in USB_HostMidiRecv\r\n");
            }
            break;

        case kUSB_HostMidiRunPrimeDataReceive: /* receive data */
            midiInstance->runWaitState = kUSB_HostMidiRunWaitDataReceived;
            midiInstance->runState     = kUSB_HostMidiRunIdle;
            if (USB_HostMidiRecv(midiInstance->classHandle, midiInstance->midiRxBuffer,
                                 midiInstance->bulkInMaxPacketSize, USB_HostMidiInCallback,
                                 midiInstance) != kStatus_USB_Success)
            {
                usb_echo("error in USB_HostMidiRecv\r\n");
            }
            break;

        default:
            break;
    }
}

usb_status_t USB_HostMidiEvent(usb_device_handle deviceHandle,
                               usb_host_configuration_handle configurationHandle,
                               uint32_t eventCode)
{
    usb_host_configuration_t *configuration;
    usb_host_interface_t *interface;
    uint32_t infoValue  = 0U;
    usb_status_t status = kStatus_USB_Success;
    uint8_t interfaceIndex;
    uint8_t id;

    switch (eventCode)
    {
        case kUSB_HostEventAttach:
            /* judge whether is configurationHandle supported */
            configuration = (usb_host_configuration_t *)configurationHandle;
            for (interfaceIndex = 0; interfaceIndex < configuration->interfaceCount; ++interfaceIndex)
            {
                interface = &configuration->interfaceList[interfaceIndex];
                id        = interface->interfaceDesc->bInterfaceClass;
                if (id != USB_HOST_MIDI_CLASS_CODE)
                {
                    continue;
                }
                id = interface->interfaceDesc->bInterfaceSubClass;
                if (id != USB_HOST_MIDI_SUBCLASS_CODE)
                {
                    continue;
                }
                id = interface->interfaceDesc->bInterfaceProtocol;
                if (id != USB_HOST_MIDI_PROTOCOL_CODE)
                {
                    continue;
                }
                else
                {
                    if (g_HostMidi.deviceState == kStatus_DEV_Idle)
                    {
                        /* the interface is supported by the application */
                        g_HostMidi.midiRxBuffer    = s_midiRxBuffer;
                        g_HostMidi.midiTxBuffer    = s_midiTxBuffer;
                        g_HostMidi.deviceHandle    = deviceHandle;
                        g_HostMidi.interfaceHandle = interface;
                        g_HostMidi.configHandle    = configurationHandle;
                        return kStatus_USB_Success;
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            status = kStatus_USB_NotSupported;
            break;

        case kUSB_HostEventNotSupported:
            break;

        case kUSB_HostEventEnumerationDone:
            if (g_HostMidi.configHandle == configurationHandle)
            {
                if ((g_HostMidi.deviceHandle != NULL) && (g_HostMidi.interfaceHandle != NULL))
                {
                    /* the device enumeration is done */
                    if (g_HostMidi.deviceState == kStatus_DEV_Idle)
                    {
                        g_HostMidi.deviceState = kStatus_DEV_Attached;

                        USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDevicePID, &infoValue);
                        usb_echo("midi attached:pid=0x%x ", infoValue);
                        USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDeviceVID, &infoValue);
                        usb_echo("vid=0x%x ", infoValue);
                        USB_HostHelperGetPeripheralInformation(deviceHandle, kUSB_HostGetDeviceAddress, &infoValue);
                        usb_echo("address=%d\r\n", infoValue);
                        USB_HostAppWakeUp();
                    }
                    else
                    {
                        usb_echo("not idle host midi instance\r\n");
                        status = kStatus_USB_Error;
                    }
                }
            }
            break;

        case kUSB_HostEventDetach:
            if (g_HostMidi.configHandle == configurationHandle)
            {
                /* the device is detached */
                g_HostMidi.configHandle = NULL;
                if (g_HostMidi.deviceState != kStatus_DEV_Idle)
                {
                    g_HostMidi.deviceState = kStatus_DEV_Detached;
                    g_HostMidi.attachFlag = 0;
                    USB_HostAppWakeUp();
                }
            }
            break;

        default:
            break;
    }
    return status;
}
