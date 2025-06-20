/*
 * usb_host_midi.c
 *
 *  Created on: 2025/06/17
 *      Author: M.Akino
 */

#include "usb_host_config.h"
#if ((defined USB_HOST_CONFIG_MIDI) && (USB_HOST_CONFIG_MIDI))
#include "usb_host.h"
#include "usb_host_midi.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief midi in pipe transfer callback.
 *
 * @param param       callback parameter.
 * @param transfer    callback transfer.
 * @param status      transfer status.
 */
static void USB_HostMidiInPipeCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief midi out pipe transfer callback.
 *
 * @param param       callback parameter.
 * @param transfer    callback transfer.
 * @param status      transfer status.
 */
static void USB_HostMidiOutPipeCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

static void USB_HostMidiInPipeCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)param;

#if ((defined USB_HOST_CONFIG_CLASS_AUTO_CLEAR_STALL) && USB_HOST_CONFIG_CLASS_AUTO_CLEAR_STALL)
    if (status == kStatus_USB_TransferStall)
    {
        if (USB_HostMidiClearHalt(hidInstance, transfer, USB_HostMidiClearInHaltCallback,
                                  (USB_REQUEST_TYPE_DIR_IN |
                                   ((usb_host_pipe_t *)mifiInstance->inPipe)->endpointAddress)) == kStatus_USB_Success)
        {
            (void)USB_HostFreeTransfer(hidInstance->hostHandle, transfer);
            return;
        }
    }
#endif
    if (midiInstance->inCallbackFn != NULL)
    {
        /* callback to application, callback function is initialized in the USB_HostMidiRecv */
        midiInstance->inCallbackFn(midiInstance->inCallbackParam, transfer->transferBuffer, transfer->transferSofar,
                                   status);
    }
    (void)USB_HostFreeTransfer(midiInstance->hostHandle, transfer);
}

static void USB_HostMidiOutPipeCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)param;

#if ((defined USB_HOST_CONFIG_CLASS_AUTO_CLEAR_STALL) && USB_HOST_CONFIG_CLASS_AUTO_CLEAR_STALL)
    if (status == kStatus_USB_TransferStall)
    {
        if (USB_HostMidiClearHalt(midiInstance, transfer, USB_HostMidiClearOutHaltCallback,
                                  (USB_REQUEST_TYPE_DIR_OUT |
                                   ((usb_host_pipe_t *)midiInstance->outPipe)->endpointAddress)) == kStatus_USB_Success)
        {
            (void)USB_HostFreeTransfer(hidInstance->hostHandle, transfer);
            return;
        }
    }
#endif
    if (midiInstance->outCallbackFn != NULL)
    {
        /* callback to application, callback function is initialized in USB_HostMidiSend */
        midiInstance->outCallbackFn(midiInstance->outCallbackParam, transfer->transferBuffer, transfer->transferSofar,
                                    status); /* callback to application */
    }
    (void)USB_HostFreeTransfer(midiInstance->hostHandle, transfer);
}

static usb_status_t USB_HostMidiOpenInterface(usb_host_midi_instance_t *midiInstance)
{
    usb_status_t status;
    uint8_t epIndex = 0;
    usb_host_pipe_init_t pipeInit;
    usb_descriptor_endpoint_t *epDesc = NULL;
    usb_host_interface_t *interfacePointer;

    if (midiInstance->inPipe != NULL) /* close bulk in pipe if it is open */
    {
        status = USB_HostClosePipe(midiInstance->hostHandle, midiInstance->inPipe);

        if (status != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error when close pipe\r\n");
#endif
        }
        midiInstance->inPipe = NULL;
    }
    if (midiInstance->outPipe != NULL) /* close bulk out pipe if it is open */
    {
        status = USB_HostClosePipe(midiInstance->hostHandle, midiInstance->outPipe);

        if (status != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error when close pipe\r\n");
#endif
        }
        midiInstance->outPipe = NULL;
    }

    /* open interface pipes */
    interfacePointer = (usb_host_interface_t *)midiInstance->interfaceHandle;
    for (epIndex = 0; epIndex < interfacePointer->epCount; ++epIndex)
    {
        epDesc = interfacePointer->epList[epIndex].epDesc;
        if (((epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) ==
             USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN) &&
            ((epDesc->bmAttributes & USB_DESCRIPTOR_ENDPOINT_ATTRIBUTE_TYPE_MASK) == USB_ENDPOINT_BULK))
        {
            pipeInit.devInstance     = midiInstance->deviceHandle;
            pipeInit.pipeType        = USB_ENDPOINT_BULK;
            pipeInit.direction       = USB_IN;
            pipeInit.endpointAddress = (epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK);
            pipeInit.interval        = epDesc->bInterval;
            pipeInit.maxPacketSize   = (uint16_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                 USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_SIZE_MASK));
            pipeInit.numberPerUframe = (uint8_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                  USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_MASK));
            pipeInit.nakCount        = USB_HOST_CONFIG_MAX_NAK;

            midiInstance->inPacketSize = pipeInit.maxPacketSize;

            status = USB_HostOpenPipe(midiInstance->hostHandle, &midiInstance->inPipe, &pipeInit);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("USB_HostMidiSetInterface fail to open pipe\r\n");
#endif
                return kStatus_USB_Error;
            }
        }
        else if (((epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) ==
                  USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT) &&
                 ((epDesc->bmAttributes & USB_DESCRIPTOR_ENDPOINT_ATTRIBUTE_TYPE_MASK) == USB_ENDPOINT_BULK))
        {
            pipeInit.devInstance     = midiInstance->deviceHandle;
            pipeInit.pipeType        = USB_ENDPOINT_BULK;
            pipeInit.direction       = USB_OUT;
            pipeInit.endpointAddress = (epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK);
            pipeInit.interval        = epDesc->bInterval;
            pipeInit.maxPacketSize   = (uint16_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                 USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_SIZE_MASK));
            pipeInit.numberPerUframe = (uint8_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                  USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_MASK));
            pipeInit.nakCount        = USB_HOST_CONFIG_MAX_NAK;

            midiInstance->outPacketSize = pipeInit.maxPacketSize;

            status = USB_HostOpenPipe(midiInstance->hostHandle, &midiInstance->outPipe, &pipeInit);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("USB_HostMidiSetInterface fail to open pipe\r\n");
#endif
                return kStatus_USB_Error;
            }
        }
        else
        {
        }
    }

    return kStatus_USB_Success;
}

static void USB_HostMidiSetInterfaceCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)param;

    midiInstance->controlTransfer = NULL;
    if (status == kStatus_USB_Success)
    {
        status = USB_HostMidiOpenInterface(midiInstance); /* midi open interface */
    }

    if (midiInstance->controlCallbackFn != NULL)
    {
        /* callback to application, callback function is initialized in the USB_HostMidiSetInterface
        or USB_HostMidiControl, but is the same function */
        midiInstance->controlCallbackFn(midiInstance->controlCallbackParam, NULL, 0, status);
    }
    (void)USB_HostFreeTransfer(midiInstance->hostHandle, transfer);
}

usb_status_t USB_HostMidiInit(usb_device_handle deviceHandle, usb_host_class_handle *classHandle)
{
    uint32_t infoValue = 0U;
    uint32_t *temp;
    usb_host_midi_instance_t *midiInstance =
        (usb_host_midi_instance_t *)OSA_MemoryAllocate(sizeof(usb_host_midi_instance_t)); /* malloc midi class instance */

    if (midiInstance == NULL)
    {
        return kStatus_USB_AllocFail;
    }

    /* initialize hid instance */
    midiInstance->deviceHandle    = deviceHandle;
    midiInstance->interfaceHandle = NULL;
    (void)USB_HostHelperGetPeripheralInformation(deviceHandle, (uint32_t)kUSB_HostGetHostHandle, &infoValue);
    temp                     = (uint32_t *)infoValue;
    midiInstance->hostHandle = (usb_host_handle)temp;
    (void)USB_HostHelperGetPeripheralInformation(deviceHandle, (uint32_t)kUSB_HostGetDeviceControlPipe, &infoValue);
    temp                      = (uint32_t *)infoValue;
    midiInstance->controlPipe = (usb_host_pipe_handle)temp;

    *classHandle = midiInstance;
    return kStatus_USB_Success;
}

usb_status_t USB_HostMidiSetInterface(usb_host_class_handle classHandle,
                                      usb_host_interface_handle interfaceHandle,
                                      uint8_t alternateSetting,
                                      transfer_callback_t callbackFn,
                                      void *callbackParam)
{
    usb_status_t status;
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)classHandle;
    usb_host_transfer_t *transfer;

    if (classHandle == NULL)
    {
        return kStatus_USB_InvalidParameter;
    }

    midiInstance->interfaceHandle = interfaceHandle;
    status                        = USB_HostOpenDeviceInterface(midiInstance->deviceHandle,
                                          interfaceHandle); /* notify host driver the interface is open */
    if (status != kStatus_USB_Success)
    {
        return status;
    }

    /* cancel transfers */
    if (midiInstance->inPipe != NULL)
    {
        status = USB_HostCancelTransfer(midiInstance->hostHandle, midiInstance->inPipe, NULL);

        if (status != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error when cancel pipe\r\n");
#endif
        }
    }
    if (midiInstance->outPipe != NULL)
    {
        status = USB_HostCancelTransfer(midiInstance->hostHandle, midiInstance->outPipe, NULL);

        if (status != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error when cancel pipe\r\n");
#endif
        }
    }

    if (alternateSetting == 0U) /* open interface directly */
    {
        if (callbackFn != NULL)
        {
            status = USB_HostMidiOpenInterface(midiInstance);
            callbackFn(callbackParam, NULL, 0, status);
        }
    }
    else /* send setup transfer */
    {
        /* malloc one transfer */
        if (USB_HostMallocTransfer(midiInstance->hostHandle, &transfer) != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error to get transfer\r\n");
#endif
            return kStatus_USB_Error;
        }

        /* save the application callback function */
        midiInstance->controlCallbackFn    = callbackFn;
        midiInstance->controlCallbackParam = callbackParam;
        /* initialize transfer */
        transfer->callbackFn                 = USB_HostMidiSetInterfaceCallback;
        transfer->callbackParam              = midiInstance;
        transfer->setupPacket->bRequest      = USB_REQUEST_STANDARD_SET_INTERFACE;
        transfer->setupPacket->bmRequestType = USB_REQUEST_TYPE_RECIPIENT_INTERFACE;
        transfer->setupPacket->wIndex        = USB_SHORT_TO_LITTLE_ENDIAN(
            ((usb_host_interface_t *)midiInstance->interfaceHandle)->interfaceDesc->bInterfaceNumber);
        transfer->setupPacket->wValue  = USB_SHORT_TO_LITTLE_ENDIAN(alternateSetting);
        transfer->setupPacket->wLength = 0U;
        transfer->transferBuffer       = NULL;
        transfer->transferLength       = 0U;
        status                         = USB_HostSendSetup(midiInstance->hostHandle, midiInstance->controlPipe, transfer);

        if (status == kStatus_USB_Success)
        {
            midiInstance->controlTransfer = transfer;
        }
        else
        {
            (void)USB_HostFreeTransfer(midiInstance->hostHandle, transfer);
        }
    }

    return status;
}

usb_status_t USB_HostMidiDeinit(usb_device_handle deviceHandle, usb_host_class_handle classHandle)
{
    usb_status_t status;
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)classHandle;

    if (deviceHandle == NULL)
    {
        return kStatus_USB_InvalidHandle;
    }

    if (classHandle != NULL) /* class instance has initialized */
    {
        if (midiInstance->inPipe != NULL)
        {
            status = USB_HostCancelTransfer(midiInstance->hostHandle, midiInstance->inPipe, NULL); /* cancel pipe */
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when cancel pipe\r\n");
#endif
            }
            status = USB_HostClosePipe(midiInstance->hostHandle, midiInstance->inPipe); /* close pipe */

            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when close pipe\r\n");
#endif
            }
            midiInstance->inPipe = NULL;
        }
        if (midiInstance->outPipe != NULL)
        {
            status = USB_HostCancelTransfer(midiInstance->hostHandle, midiInstance->outPipe, NULL); /* cancel pipe */
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when cancel pipe\r\n");
#endif
            }
            status = USB_HostClosePipe(midiInstance->hostHandle, midiInstance->outPipe); /* close pipe */

            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when close pipe\r\n");
#endif
            }
            midiInstance->outPipe = NULL;
        }
        if ((midiInstance->controlPipe != NULL) &&
            (midiInstance->controlTransfer != NULL)) /* cancel control transfer if there is on-going control transfer */
        {
            status =
                USB_HostCancelTransfer(midiInstance->hostHandle, midiInstance->controlPipe, midiInstance->controlTransfer);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when cancel pipe\r\n");
#endif
            }
        }
        (void)USB_HostCloseDeviceInterface(
            deviceHandle, midiInstance->interfaceHandle); /* notify host driver the interface is closed */
        OSA_MemoryFree(midiInstance);
    }
    else
    {
        (void)USB_HostCloseDeviceInterface(deviceHandle, NULL);
    }

    return kStatus_USB_Success;
}

uint16_t USB_HostMidiGetPacketsize(usb_host_class_handle classHandle, uint8_t pipeType, uint8_t direction)
{
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)classHandle;
    if (classHandle == NULL)
    {
        return 0U;
    }

    if (pipeType == USB_ENDPOINT_BULK)
    {
        if (direction == USB_IN)
        {
            return midiInstance->inPacketSize;
        }
        else
        {
            return midiInstance->outPacketSize;
        }
    }

    return 0;
}

usb_status_t USB_HostMidiRecv(usb_host_class_handle classHandle,
                              uint8_t *buffer,
                              uint32_t bufferLength,
                              transfer_callback_t callbackFn,
                              void *callbackParam)
{
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)classHandle;
    usb_host_transfer_t *transfer;

    if (classHandle == NULL)
    {
        return kStatus_USB_InvalidHandle;
    }

    if (midiInstance->inPipe == NULL)
    {
        return kStatus_USB_Error;
    }

    /* malloc one transfer */
    if (USB_HostMallocTransfer(midiInstance->hostHandle, &transfer) != kStatus_USB_Success)
    {
#ifdef HOST_ECHO
        usb_echo("error to get transfer\r\n");
#endif
        return kStatus_USB_Busy;
    }
    /* save the application callback function */
    midiInstance->inCallbackFn    = callbackFn;
    midiInstance->inCallbackParam = callbackParam;
    /* initialize transfer */
    transfer->transferBuffer = buffer;
    transfer->transferLength = bufferLength;
    transfer->callbackFn     = USB_HostMidiInPipeCallback;
    transfer->callbackParam  = midiInstance;

    if (USB_HostRecv(midiInstance->hostHandle, midiInstance->inPipe, transfer) !=
        kStatus_USB_Success) /* call host driver api */
    {
#ifdef HOST_ECHO
        usb_echo("failed to USB_HostRecv\r\n");
#endif
        (void)USB_HostFreeTransfer(midiInstance->hostHandle, transfer);
        return kStatus_USB_Error;
    }

    return kStatus_USB_Success;
}

usb_status_t USB_HostMidiSend(usb_host_class_handle classHandle,
                              uint8_t *buffer,
                              uint32_t bufferLength,
                              transfer_callback_t callbackFn,
                              void *callbackParam)
{
    usb_host_midi_instance_t *midiInstance = (usb_host_midi_instance_t *)classHandle;
    usb_host_transfer_t *transfer;

    if (classHandle == NULL)
    {
        return kStatus_USB_InvalidHandle;
    }

    if (midiInstance->outPipe == NULL)
    {
        return kStatus_USB_Error;
    }

    /* malloc one transfer */
    if (USB_HostMallocTransfer(midiInstance->hostHandle, &transfer) != kStatus_USB_Success)
    {
#ifdef HOST_ECHO
        usb_echo("error to get transfer\r\n");
#endif
        return kStatus_USB_Error;
    }
    /* save the application callback function */
    midiInstance->outCallbackFn    = callbackFn;
    midiInstance->outCallbackParam = callbackParam;
    /* initialize transfer */
    transfer->transferBuffer = buffer;
    transfer->transferLength = bufferLength;
    transfer->callbackFn     = USB_HostMidiOutPipeCallback;
    transfer->callbackParam  = midiInstance;

    if (USB_HostSend(midiInstance->hostHandle, midiInstance->outPipe, transfer) !=
        kStatus_USB_Success) /* call host driver api */
    {
#ifdef HOST_ECHO
        usb_echo("failed to USB_HostSend\r\n");
#endif
        (void)USB_HostFreeTransfer(midiInstance->hostHandle, transfer);
        return kStatus_USB_Error;
    }

    return kStatus_USB_Success;
}

#endif /* USB_HOST_CONFIG_HID */
