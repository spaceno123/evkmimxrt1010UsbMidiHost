/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016, 2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "usb_host_config.h"
#if ((defined USB_HOST_CONFIG_MSD) && (USB_HOST_CONFIG_MSD))
#include "usb_host.h"
#include "usb_host_msd.h"
#include "usb_host_hci.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief clear stall transfer callback.
 *
 * @param param    callback parameter.
 * @param transfer transfer.
 * @param status   transfer result status.
 */
static void USB_HostMsdClearHaltCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief send clear stall transfer.
 *
 * @param msdInstance     msd instance pointer.
 * @param callbackFn  callback function.
 * @param endpoint    endpoint address.
 *
 * @return An error code or kStatus_USB_Success.
 */
static usb_status_t USB_HostMsdClearHalt(usb_host_msd_instance_t *msdInstance,
                                         host_inner_transfer_callback_t callbackFn,
                                         uint8_t endpoint);

/*!
 * @brief mass storage reset three step processes are done.
 *
 * @param msdInstance     msd instance pointer.
 * @param status      result status.
 */
static void USB_HostMsdResetDone(usb_host_msd_instance_t *msdInstance, usb_status_t status);

/*!
 * @brief mass storage reset process step 3 callback.
 *
 * @param msdInstance msd instance pointer.
 * @param transfer     transfer
 * @param status       result status.
 */
static void USB_HostMsdMassResetClearOutCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief mass storage reset process step 2 callback.
 *
 * @param msdInstance msd instance pointer.
 * @transfer           transfer
 * @param status       result status.
 */
static void USB_HostMsdMassResetClearInCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief mass storage reset process step 1 callback.
 *
 * @param msdInstance   msd instance pointer.
 * @param transfer       transfer
 * @param status         result status.
 */
static void USB_HostMsdMassResetCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief mass storage control transfer callback function.
 *
 * @param msdInstance   msd instance pointer.
 * @param transfer       transfer
 * @param status         result status.
 */
static void USB_HostMsdControlCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief this function is called when ufi command is done.
 *
 * @param msdInstance     msd instance pointer.
 */
static void USB_HostMsdCommandDone(usb_host_msd_instance_t *msdInstance, usb_status_t status);

/*!
 * @brief csw transfer callback.
 *
 * @param msdInstance   msd instance pointer.
 * @param transfer       transfer
 * @param status         result status.
 */
static void USB_HostMsdCswCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief cbw transfer callback.
 *
 * @param msdInstance   msd instance pointer.
 * @param transfer       transfer
 * @param status         result status.
 */
static void USB_HostMsdCbwCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief data transfer callback.
 *
 * @param msdInstance   sd instance pointer.
 * @param transfer       transfer
 * @param status         result status.
 */
static void USB_HostMsdDataCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief msd open interface.
 *
 * @param msdInstance     msd instance pointer.
 *
 * @return kStatus_USB_Success or error codes.
 */
static usb_status_t USB_HostMsdOpenInterface(usb_host_msd_instance_t *msdInstance);

/*!
 * @brief msd set interface callback, open pipes.
 *
 * @param param       callback parameter.
 * @param transfer    callback transfer.
 * @param status      transfer status.
 */
static void USB_HostMsdSetInterfaceCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status);

/*!
 * @brief msd control transfer common code.
 *
 * This function allocate the resource for msd instance.
 *
 * @param msdInstance          the msd class instance.
 * @param pipeCallbackFn       inner callback function.
 * @param callbackFn           callback function.
 * @param callbackParam        callback parameter.
 * @param buffer               buffer pointer.
 * @param bufferLength         buffer length.
 * @param requestType          request type.
 * @param requestValue         request value.
 *
 * @return An error code or kStatus_USB_Success.
 */
static usb_status_t USB_HostMsdControl(usb_host_msd_instance_t *msdInstance,
                                       host_inner_transfer_callback_t pipeCallbackFn,
                                       transfer_callback_t callbackFn,
                                       void *callbackParam,
                                       uint8_t *buffer,
                                       uint16_t bufferLength,
                                       uint8_t requestType,
                                       uint8_t requestValue);

/*!
 * @brief command process function, this function is called many time for one command's different state.
 *
 * @param msdInstance          the msd class instance.
 *
 * @return An error code or kStatus_USB_Success.
 */
static usb_status_t USB_HostMsdProcessCommand(usb_host_msd_instance_t *msdInstance);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

static void USB_HostMsdClearHaltCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    if (status != kStatus_USB_Success)
    {
        USB_HostMsdCommandDone(msdInstance, kStatus_USB_TransferCancel);
    }

    if (msdInstance->commandStatus == (uint8_t)kMSD_CommandErrorDone)
    {
        USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error); /* command fail */
    }
    else
    {
        (void)USB_HostMsdProcessCommand(msdInstance); /* continue to process ufi command */
    }
}

static usb_status_t USB_HostMsdClearHalt(usb_host_msd_instance_t *msdInstance,
                                         host_inner_transfer_callback_t callbackFn,
                                         uint8_t endpoint)
{
    usb_status_t status;
    usb_host_transfer_t *transfer;

    /* malloc one transfer */
    status = USB_HostMallocTransfer(msdInstance->hostHandle, &transfer);
    if (status != kStatus_USB_Success)
    {
#ifdef HOST_ECHO
        usb_echo("allocate transfer error\r\n");
#endif
        return status;
    }

    /* initialize transfer */
    transfer->callbackFn                 = callbackFn;
    transfer->callbackParam              = msdInstance;
    transfer->transferBuffer             = NULL;
    transfer->transferLength             = 0;
    transfer->setupPacket->bRequest      = USB_REQUEST_STANDARD_CLEAR_FEATURE;
    transfer->setupPacket->bmRequestType = USB_REQUEST_TYPE_RECIPIENT_ENDPOINT;
    transfer->setupPacket->wValue  = USB_SHORT_TO_LITTLE_ENDIAN(USB_REQUEST_STANDARD_FEATURE_SELECTOR_ENDPOINT_HALT);
    transfer->setupPacket->wIndex  = USB_SHORT_TO_LITTLE_ENDIAN(endpoint);
    transfer->setupPacket->wLength = 0;
    status                         = USB_HostSendSetup(msdInstance->hostHandle, msdInstance->controlPipe, transfer);

    if (status != kStatus_USB_Success)
    {
        (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);
    }
    msdInstance->controlTransfer = transfer;

    return status;
}

static void USB_HostMsdResetDone(usb_host_msd_instance_t *msdInstance, usb_status_t status)
{
    if (msdInstance->internalResetRecovery == 1U) /* internal mass reset recovery */
    {
        msdInstance->internalResetRecovery = 0U;

        if ((status != kStatus_USB_Success) || (msdInstance->commandStatus == (uint8_t)kMSD_CommandErrorDone))
        {
            USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error); /* command fail */
        }
        else
        {
            (void)USB_HostMsdProcessCommand(msdInstance); /* continue to process ufi command */
        }
    }
    else /* user call mass storage reset recovery */
    {
        if (msdInstance->controlCallbackFn != NULL)
        {
            /* callback to application, callback function is initialized in the USB_HostMsdControl,
            or USB_HostMsdSetInterface, but is the same function */
            msdInstance->controlCallbackFn(msdInstance->controlCallbackParam, NULL, 0,
                                           status); /* callback to application */
        }
    }
}

static void USB_HostMsdMassResetClearOutCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    msdInstance->controlTransfer = NULL;
    (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);
    USB_HostMsdResetDone(msdInstance, status); /* mass storage reset done */
}

static void USB_HostMsdMassResetClearInCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    msdInstance->controlTransfer = NULL;
    (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);

    if (status == kStatus_USB_Success)
    {
        if (msdInstance->outPipe != NULL)
        {
            /* continue to process mass storage reset */
            (void)USB_HostMsdClearHalt(
                msdInstance, USB_HostMsdMassResetClearOutCallback,
                (USB_REQUEST_TYPE_DIR_OUT | ((usb_host_pipe_t *)msdInstance->outPipe)->endpointAddress));
        }
    }
    else
    {
        USB_HostMsdResetDone(msdInstance, status); /* mass storage reset done */
    }
}

static void USB_HostMsdMassResetCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    msdInstance->controlTransfer = NULL;
    (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);
    if (status == kStatus_USB_Success)
    {
        if (msdInstance->inPipe != NULL)
        {
            /* continue to process mass storage reset */
            (void)USB_HostMsdClearHalt(
                msdInstance, USB_HostMsdMassResetClearInCallback,
                (USB_REQUEST_TYPE_DIR_IN | ((usb_host_pipe_t *)msdInstance->inPipe)->endpointAddress));
        }
    }
    else
    {
        USB_HostMsdResetDone(msdInstance, status); /* mass storage reset done */
    }
}

static void USB_HostMsdControlCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    msdInstance->controlTransfer = NULL;
    if (msdInstance->controlCallbackFn != NULL)
    {
        /* callback to application, callback function is initialized in the USB_HostMsdControl,
        or USB_HostMsdSetInterface, but is the same function */
        msdInstance->controlCallbackFn(msdInstance->controlCallbackParam, transfer->transferBuffer,
                                       transfer->transferSofar, status); /* callback to application */
    }
    (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);
}

static void USB_HostMsdCommandDone(usb_host_msd_instance_t *msdInstance, usb_status_t status)
{
    msdInstance->commandStatus = (uint8_t)kMSD_CommandIdle;
    if (msdInstance->commandCallbackFn != NULL)
    {
        /* callback to application, the callback function is initialized in USB_HostMsdCommand */
        msdInstance->commandCallbackFn(msdInstance->commandCallbackParam, msdInstance->msdCommand.dataBuffer,
                                       msdInstance->msdCommand.dataSofar, status);
    }
}

static void USB_HostMsdCswCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    if (status == kStatus_USB_Success)
    {
        /* kStatus_USB_Success */
        if ((transfer->transferSofar == USB_HOST_UFI_CSW_LENGTH) &&
            (msdInstance->msdCommand.cswBlock->CSWSignature == USB_LONG_TO_LITTLE_ENDIAN(USB_HOST_MSD_CSW_SIGNATURE)))
        {
            switch (msdInstance->msdCommand.cswBlock->CSWStatus)
            {
                case 0:
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_Success);
                    break;

                case 1:
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_MSDStatusFail);
                    break;

                case 2:
                    msdInstance->internalResetRecovery = 1U;
                    msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
                    if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
                    {
                        USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                    }
                    break;

                default:
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_MSDStatusFail);
                    break;
            }
        }
        else
        {
            /* mass reset recovery to end ufi command */
            msdInstance->internalResetRecovery = 1U;
            msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
            if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
            {
                USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
            }
        }
    }
    else
    {
        if (status == kStatus_USB_TransferStall) /* case 1: stall */
        {
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                msdInstance->msdCommand.retryTime--; /* retry reduce when error */
            }
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                /* clear stall to continue the ufi command */
                if (USB_HostMsdClearHalt(
                        msdInstance, USB_HostMsdClearHaltCallback,
                        (USB_REQUEST_TYPE_DIR_IN | ((usb_host_pipe_t *)msdInstance->inPipe)->endpointAddress)) !=
                    kStatus_USB_Success)
                {
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                }
            }
            else
            {
                /* mass reset recovery to continue ufi command */
                msdInstance->internalResetRecovery = 1U;
                msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
                if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
                {
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                }
            }
        }
        else if (status == kStatus_USB_TransferCancel) /* case 2: cancel */
        {
            USB_HostMsdCommandDone(msdInstance, status); /* command cancel */
        }
        else /* case 3: error */
        {
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                msdInstance->msdCommand.retryTime--; /* retry reduce when error */
            }
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                (void)USB_HostMsdProcessCommand(msdInstance); /* retry the last step transaction */
            }
            else
            {
                /* mass reset recovery to continue ufi command */
                msdInstance->internalResetRecovery = 1U;
                msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
                if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
                {
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                }
            }
        }
    }
}

static void USB_HostMsdCbwCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    if (status == kStatus_USB_Success)
    {
        /* kStatus_USB_Success */
        if (transfer->transferSofar == USB_HOST_UFI_CBW_LENGTH)
        {
            if (NULL != msdInstance->msdCommand.dataBuffer)
            {
                msdInstance->commandStatus = (uint8_t)kMSD_CommandTransferData;
            }
            else
            {
                msdInstance->commandStatus = (uint8_t)kMSD_CommandTransferCSW;
            }

            (void)USB_HostMsdProcessCommand(msdInstance); /* continue to process ufi command */
        }
        else
        {
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                msdInstance->msdCommand.retryTime--;
            }
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                (void)USB_HostMsdProcessCommand(msdInstance); /* retry the last step transaction */
            }
            else
            {
                /* mass reset recovery to continue ufi command */
                msdInstance->internalResetRecovery = 1U;
                msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
                if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
                {
                    (void)USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                }
            }
        }
    }
    else
    {
        if (status == kStatus_USB_TransferStall) /* case 1: stall */
        {
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                msdInstance->msdCommand.retryTime--; /* retry reduce when error */
            }
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                /* clear stall to continue the ufi command */
                if (USB_HostMsdClearHalt(
                        msdInstance, USB_HostMsdClearHaltCallback,
                        (USB_REQUEST_TYPE_DIR_OUT | ((usb_host_pipe_t *)msdInstance->inPipe)->endpointAddress)) !=
                    kStatus_USB_Success)
                {
                    (void)USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                }
            }
            else
            {
                /* mass reset recovery to continue ufi command */
                msdInstance->internalResetRecovery = 1U;
                msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
                if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
                {
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                }
            }
        }
        else if (status == kStatus_USB_TransferCancel) /* case 2: cancel */
        {
            USB_HostMsdCommandDone(msdInstance, status); /* command cancel */
        }
        else /* case 3: error */
        {
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                msdInstance->msdCommand.retryTime--;
            }
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                (void)USB_HostMsdProcessCommand(msdInstance); /* retry the last step transaction */
            }
            else
            {
                /* mass reset recovery to continue ufi command */
                msdInstance->internalResetRecovery = 1U;
                msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
                if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
                {
                    USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
                }
            }
        }
        return;
    }
}

static void USB_HostMsdDataCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;
    uint8_t direction;

    if (status == kStatus_USB_Success)
    {
        /* kStatus_USB_Success */
        msdInstance->msdCommand.dataSofar += transfer->transferSofar;
        if (msdInstance->msdCommand.dataSofar >= msdInstance->msdCommand.dataLength)
        {
            msdInstance->commandStatus = (uint8_t)kMSD_CommandTransferCSW; /* next step */
        }

        (void)USB_HostMsdProcessCommand(msdInstance); /* continue to process ufi command */
    }
    else
    {
        if (status == kStatus_USB_TransferStall) /* case 1: stall */
        {
            if (msdInstance->msdCommand.retryTime > 0U)
            {
                msdInstance->msdCommand.retryTime--; /* retry reduce when error */
            }
            if (transfer->direction == USB_IN)
            {
                direction = USB_REQUEST_TYPE_DIR_IN;
            }
            else
            {
                direction = USB_REQUEST_TYPE_DIR_OUT;
            }

            if (msdInstance->msdCommand.retryTime == 0U)
            {
                msdInstance->commandStatus = (uint8_t)kMSD_CommandTransferCSW; /* next step */
            }
            /* clear stall to continue the ufi command */
            if (USB_HostMsdClearHalt(msdInstance, USB_HostMsdClearHaltCallback,
                                     (direction | ((usb_host_pipe_t *)msdInstance->inPipe)->endpointAddress)) !=
                kStatus_USB_Success)
            {
                USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
            }
        }
        else if (status == kStatus_USB_TransferCancel) /* case 2: cancel */
        {
            USB_HostMsdCommandDone(msdInstance, status); /* command cancel */
        }
        else /* case 3: error */
        {
            /* mass reset recovery to finish ufi command */
            msdInstance->internalResetRecovery = 1U;
            msdInstance->commandStatus         = (uint8_t)kMSD_CommandErrorDone;
            if (USB_HostMsdMassStorageReset(msdInstance, NULL, NULL) != kStatus_USB_Success)
            {
                USB_HostMsdCommandDone(msdInstance, kStatus_USB_Error);
            }
        }
    }
}

static usb_status_t USB_HostMsdProcessCommand(usb_host_msd_instance_t *msdInstance)
{
    usb_status_t status = kStatus_USB_Success;
    usb_host_transfer_t *transfer;
    usb_host_msd_command_status_t commandStatus;
    if (msdInstance->msdCommand.transfer == NULL)
    {
        /* malloc one transfer */
        status = USB_HostMallocTransfer(msdInstance->hostHandle, &(msdInstance->msdCommand.transfer));
        if (status != kStatus_USB_Success)
        {
            msdInstance->msdCommand.transfer = NULL;
#ifdef HOST_ECHO
            usb_echo("allocate transfer error\r\n");
#endif
            return kStatus_USB_Busy;
        }
    }
    transfer = msdInstance->msdCommand.transfer;

    commandStatus = (usb_host_msd_command_status_t)msdInstance->commandStatus;
    switch (commandStatus)
    {
        case kMSD_CommandTransferCBW: /* ufi CBW phase */
            transfer->direction      = USB_OUT;
            transfer->transferBuffer = (uint8_t *)msdInstance->msdCommand.cbwBlock;
            transfer->transferLength = USB_HOST_UFI_CBW_LENGTH;
            transfer->callbackFn     = USB_HostMsdCbwCallback;
            transfer->callbackParam  = msdInstance;
            status                   = USB_HostSend(msdInstance->hostHandle, msdInstance->outPipe, transfer);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("host send error\r\n");
#endif
            }
            break;

        case kMSD_CommandTransferData: /* ufi DATA phase */
            if (msdInstance->msdCommand.dataBuffer != NULL)
            {
                transfer->direction      = msdInstance->msdCommand.dataDirection;
                transfer->transferBuffer = (msdInstance->msdCommand.dataBuffer + msdInstance->msdCommand.dataSofar);
                transfer->transferLength = (msdInstance->msdCommand.dataLength - msdInstance->msdCommand.dataSofar);
                transfer->callbackParam  = msdInstance;
                if (msdInstance->msdCommand.dataSofar != msdInstance->msdCommand.dataLength)
                {
                    if (transfer->direction == USB_OUT)
                    {
                        transfer->callbackFn = USB_HostMsdDataCallback;
                        status               = USB_HostSend(msdInstance->hostHandle, msdInstance->outPipe, transfer);
                        if (status != kStatus_USB_Success)
                        {
#ifdef HOST_ECHO
                            usb_echo("host send error\r\n");
#endif
                        }
                    }
                    else
                    {
                        transfer->callbackFn = USB_HostMsdDataCallback;
                        status               = USB_HostRecv(msdInstance->hostHandle, msdInstance->inPipe, transfer);
                        if (status != kStatus_USB_Success)
                        {
#ifdef HOST_ECHO
                            usb_echo("host recv error\r\n");
#endif
                        }
                    }
                }
                else
                {
                    /* for misra check */
                }
            }
            break;
        case kMSD_CommandTransferCSW: /* ufi CSW phase */
            transfer->direction      = USB_IN;
            transfer->transferBuffer = (uint8_t *)msdInstance->msdCommand.cswBlock;
            transfer->transferLength = USB_HOST_UFI_CSW_LENGTH;
            transfer->callbackFn     = USB_HostMsdCswCallback;
            transfer->callbackParam  = msdInstance;
            status                   = USB_HostRecv(msdInstance->hostHandle, msdInstance->inPipe, transfer);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("host recv error\r\n");
#endif
            }
            break;

        case kMSD_CommandDone:
            USB_HostMsdCommandDone(msdInstance, kStatus_USB_Success);
            break;

        default:
            /*no action*/
            break;
    }
    return status;
}

/*!
 * @brief all ufi function calls this api.
 *
 * This function implements the common ufi commands.
 *
 * @param classHandle   the class msd handle.
 * @param buffer         buffer pointer.
 * @param bufferLength     buffer length.
 * @param callbackFn    callback function.
 * @param callbackParam callback parameter.
 * @param direction      command direction.
 * @param byteValues    ufi command fields value.
 *
 * @return An error code or kStatus_USB_Success.
 */
usb_status_t USB_HostMsdCommand(usb_host_class_handle classHandle,
                                uint8_t *buffer,
                                uint32_t bufferLength,
                                transfer_callback_t callbackFn,
                                void *callbackParam,
                                uint8_t direction,
                                uint8_t byteValues[10])
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)classHandle;
    usb_host_cbw_t *cbwPointer           = msdInstance->msdCommand.cbwBlock;
    uint8_t index                        = 0;

    if (classHandle == NULL)
    {
        return kStatus_USB_InvalidHandle;
    }

    if (msdInstance->commandStatus != (uint8_t)kMSD_CommandIdle)
    {
        return kStatus_USB_Busy;
    }

    /* save the application callback function */
    msdInstance->commandCallbackFn    = callbackFn;
    msdInstance->commandCallbackParam = callbackParam;

    /* initialize CBWCB fields */
    for (index = 0; index < USB_HOST_UFI_BLOCK_DATA_VALID_LENGTH; ++index)
    {
        cbwPointer->CBWCB[index] = byteValues[index];
    }

    /* initialize CBW fields */
    cbwPointer->CBWDataTransferLength = USB_LONG_TO_LITTLE_ENDIAN(bufferLength);
    cbwPointer->CBWFlags              = direction;
    cbwPointer->CBWLun                = (byteValues[1] >> USB_HOST_UFI_LOGICAL_UNIT_POSITION);
    cbwPointer->CBWCBLength           = USB_HOST_UFI_BLOCK_DATA_VALID_LENGTH;

    msdInstance->commandStatus = (uint8_t)kMSD_CommandTransferCBW;
    if (direction == USB_HOST_MSD_CBW_FLAGS_DIRECTION_IN)
    {
        msdInstance->msdCommand.dataDirection = USB_IN;
    }
    else
    {
        msdInstance->msdCommand.dataDirection = USB_OUT;
    }
    msdInstance->msdCommand.dataBuffer = buffer;

    msdInstance->msdCommand.dataLength = bufferLength;
    msdInstance->msdCommand.dataSofar  = 0;
    msdInstance->msdCommand.retryTime  = USB_HOST_MSD_RETRY_MAX_TIME;

    return USB_HostMsdProcessCommand(msdInstance); /* start to process ufi command */
}

static usb_status_t USB_HostMsdOpenInterface(usb_host_msd_instance_t *msdInstance)
{
    usb_status_t status;
    uint8_t epIndex = 0;
    usb_host_pipe_init_t pipeInit;
    usb_descriptor_endpoint_t *epDesc = NULL;
    usb_host_interface_t *interfacePointer;

    if (msdInstance->inPipe != NULL) /* close bulk in pipe if the pipe is open */
    {
        status = USB_HostClosePipe(msdInstance->hostHandle, msdInstance->inPipe);
        if (status != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error when close pipe\r\n");
#endif
        }
        msdInstance->inPipe = NULL;
    }
    if (msdInstance->outPipe != NULL) /* close bulk out pipe if the pipe is open */
    {
        status = USB_HostClosePipe(msdInstance->hostHandle, msdInstance->outPipe);
        if (status != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error when close pipe\r\n");
#endif
        }
        msdInstance->outPipe = NULL;
    }

    /* open interface pipes */
    interfacePointer = (usb_host_interface_t *)msdInstance->interfaceHandle;
    for (epIndex = 0; epIndex < interfacePointer->epCount; ++epIndex)
    {
        epDesc = interfacePointer->epList[epIndex].epDesc;
        if (((epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) ==
             USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN) &&
            ((epDesc->bmAttributes & USB_DESCRIPTOR_ENDPOINT_ATTRIBUTE_TYPE_MASK) == USB_ENDPOINT_BULK))
        {
            pipeInit.devInstance     = msdInstance->deviceHandle;
            pipeInit.pipeType        = USB_ENDPOINT_BULK;
            pipeInit.direction       = USB_IN;
            pipeInit.endpointAddress = (epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK);
            pipeInit.interval        = epDesc->bInterval;
            pipeInit.maxPacketSize   = (uint16_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                 USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_SIZE_MASK));
            pipeInit.numberPerUframe = (uint8_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                  USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_MASK));
            pipeInit.nakCount        = USB_HOST_CONFIG_MAX_NAK;

            status = USB_HostOpenPipe(msdInstance->hostHandle, &msdInstance->inPipe, &pipeInit);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("usb_host_hid_set_interface fail to open pipe\r\n");
#endif
                return status;
            }
        }
        else if (((epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) ==
                  USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT) &&
                 ((epDesc->bmAttributes & USB_DESCRIPTOR_ENDPOINT_ATTRIBUTE_TYPE_MASK) == USB_ENDPOINT_BULK))
        {
            pipeInit.devInstance     = msdInstance->deviceHandle;
            pipeInit.pipeType        = USB_ENDPOINT_BULK;
            pipeInit.direction       = USB_OUT;
            pipeInit.endpointAddress = (epDesc->bEndpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK);
            pipeInit.interval        = epDesc->bInterval;
            pipeInit.maxPacketSize   = (uint16_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                 USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_SIZE_MASK));
            pipeInit.numberPerUframe = (uint8_t)((USB_SHORT_FROM_LITTLE_ENDIAN_ADDRESS(epDesc->wMaxPacketSize) &
                                                  USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_MASK));
            pipeInit.nakCount        = USB_HOST_CONFIG_MAX_NAK;

            status = USB_HostOpenPipe(msdInstance->hostHandle, &msdInstance->outPipe, &pipeInit);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("usb_host_hid_set_interface fail to open pipe\r\n");
#endif
                return status;
            }
        }
        else
        {
        }
    }

    return kStatus_USB_Success;
}

static void USB_HostMsdSetInterfaceCallback(void *param, usb_host_transfer_t *transfer, usb_status_t status)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)param;

    msdInstance->controlTransfer = NULL;
    if (status == kStatus_USB_Success)
    {
        status = USB_HostMsdOpenInterface(msdInstance); /* msd open interface */
    }

    if (msdInstance->controlCallbackFn != NULL)
    {
        /* callback to application, callback function is initialized in the USB_HostMsdControl,
        or USB_HostMsdSetInterface, but is the same function */
        msdInstance->controlCallbackFn(msdInstance->controlCallbackParam, NULL, 0,
                                       status); /* callback to application */
    }
    (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);
}

usb_status_t USB_HostMsdInit(usb_device_handle deviceHandle, usb_host_class_handle *classHandle)
{
    uint32_t infoValue = 0U;
    usb_status_t status;
    uint32_t *temp;
    usb_host_msd_instance_t *msdInstance =
        (usb_host_msd_instance_t *)OSA_MemoryAllocate(sizeof(usb_host_msd_instance_t)); /* malloc msd class instance */

    if (msdInstance == NULL)
    {
        return kStatus_USB_AllocFail;
    }

#if ((defined(USB_HOST_CONFIG_BUFFER_PROPERTY_CACHEABLE)) && (USB_HOST_CONFIG_BUFFER_PROPERTY_CACHEABLE > 0U))
    msdInstance->msdCommand.cbwBlock =
        (usb_host_cbw_t *)OSA_MemoryAllocateAlign(sizeof(usb_host_cbw_t), USB_CACHE_LINESIZE);
    msdInstance->msdCommand.cswBlock =
        (usb_host_csw_t *)OSA_MemoryAllocateAlign(sizeof(usb_host_csw_t), USB_CACHE_LINESIZE);
#else
    msdInstance->msdCommand.cbwBlock = (usb_host_cbw_t *)OSA_MemoryAllocate(sizeof(usb_host_cbw_t));
    msdInstance->msdCommand.cswBlock = (usb_host_csw_t *)OSA_MemoryAllocate(sizeof(usb_host_csw_t));
#endif

    /* initialize msd instance */
    msdInstance->deviceHandle    = deviceHandle;
    msdInstance->interfaceHandle = NULL;
    (void)USB_HostHelperGetPeripheralInformation(deviceHandle, (uint32_t)kUSB_HostGetHostHandle, &infoValue);
    temp                    = (uint32_t *)infoValue;
    msdInstance->hostHandle = (usb_host_handle)temp;
    (void)USB_HostHelperGetPeripheralInformation(deviceHandle, (uint32_t)kUSB_HostGetDeviceControlPipe, &infoValue);
    temp                                          = (uint32_t *)infoValue;
    msdInstance->controlPipe                      = (usb_host_pipe_handle)temp;
    msdInstance->msdCommand.cbwBlock->CBWSignature = USB_LONG_TO_LITTLE_ENDIAN(USB_HOST_MSD_CBW_SIGNATURE);
    status = USB_HostMallocTransfer(msdInstance->hostHandle, &(msdInstance->msdCommand.transfer));
    if (status != kStatus_USB_Success)
    {
        msdInstance->msdCommand.transfer = NULL;
#ifdef HOST_ECHO
        usb_echo("allocate transfer error\r\n");
#endif
    }

    *classHandle = msdInstance;
    return kStatus_USB_Success;
}

usb_status_t USB_HostMsdSetInterface(usb_host_class_handle classHandle,
                                     usb_host_interface_handle interfaceHandle,
                                     uint8_t alternateSetting,
                                     transfer_callback_t callbackFn,
                                     void *callbackParam)
{
    usb_status_t status;
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)classHandle;
    usb_host_transfer_t *transfer;

    if (classHandle == NULL)
    {
        return kStatus_USB_InvalidHandle;
    }

    status = USB_HostOpenDeviceInterface(msdInstance->deviceHandle,
                                         interfaceHandle); /* notify host driver the interface is open */
    if (status != kStatus_USB_Success)
    {
        return status;
    }
    msdInstance->interfaceHandle = interfaceHandle;

    /* cancel transfers */
    if (msdInstance->inPipe != NULL)
    {
        status = USB_HostCancelTransfer(msdInstance->hostHandle, msdInstance->inPipe, NULL);
        if (status != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error when cancel pipe\r\n");
#endif
        }
    }
    if (msdInstance->outPipe != NULL)
    {
        status = USB_HostCancelTransfer(msdInstance->hostHandle, msdInstance->outPipe, NULL);
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
            status = USB_HostMsdOpenInterface(msdInstance);
            callbackFn(callbackParam, NULL, 0, status);
        }
    }
    else /* send setup transfer */
    {
        /* malloc one transfer */
        if (USB_HostMallocTransfer(msdInstance->hostHandle, &transfer) != kStatus_USB_Success)
        {
#ifdef HOST_ECHO
            usb_echo("error to get transfer\r\n");
#endif
            return kStatus_USB_Busy;
        }
        /* save the application callback function */
        msdInstance->controlCallbackFn    = callbackFn;
        msdInstance->controlCallbackParam = callbackParam;
        /* initialize transfer */
        transfer->callbackFn                 = USB_HostMsdSetInterfaceCallback;
        transfer->callbackParam              = msdInstance;
        transfer->setupPacket->bRequest      = USB_REQUEST_STANDARD_SET_INTERFACE;
        transfer->setupPacket->bmRequestType = USB_REQUEST_TYPE_RECIPIENT_INTERFACE;
        transfer->setupPacket->wIndex        = USB_SHORT_TO_LITTLE_ENDIAN(
            ((usb_host_interface_t *)msdInstance->interfaceHandle)->interfaceDesc->bInterfaceNumber);
        transfer->setupPacket->wValue  = USB_SHORT_TO_LITTLE_ENDIAN(alternateSetting);
        transfer->setupPacket->wLength = 0;
        transfer->transferBuffer       = NULL;
        transfer->transferLength       = 0;
        status                         = USB_HostSendSetup(msdInstance->hostHandle, msdInstance->controlPipe, transfer);

        if (status == kStatus_USB_Success)
        {
            msdInstance->controlTransfer = transfer;
        }
        else
        {
            (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);
        }
    }

    return status;
}

usb_status_t USB_HostMsdDeinit(usb_device_handle deviceHandle, usb_host_class_handle classHandle)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)classHandle;
    usb_status_t status;

    if (classHandle != NULL)
    {
        if (msdInstance->inPipe != NULL)
        {
            status = USB_HostCancelTransfer(msdInstance->hostHandle, msdInstance->inPipe, NULL); /* cancel pipe */
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when cancel pipe\r\n");
#endif
            }
            status = USB_HostClosePipe(msdInstance->hostHandle, msdInstance->inPipe); /* close pipe */
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when close pipe\r\n");
#endif
            }
        }
        if (msdInstance->outPipe != NULL)
        {
            status = USB_HostCancelTransfer(msdInstance->hostHandle, msdInstance->outPipe, NULL); /* cancel pipe */
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when cancel pipe\r\n");
#endif
            }
            status = USB_HostClosePipe(msdInstance->hostHandle, msdInstance->outPipe); /* close pipe */
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when close pipe\r\n");
#endif
            }
        }
        if ((msdInstance->controlPipe != NULL) &&
            (msdInstance->controlTransfer != NULL)) /* cancel control transfer if there is on-going control transfer */
        {
            status =
                USB_HostCancelTransfer(msdInstance->hostHandle, msdInstance->controlPipe, msdInstance->controlTransfer);
            if (status != kStatus_USB_Success)
            {
#ifdef HOST_ECHO
                usb_echo("error when cancel pipe\r\n");
#endif
            }
        }
        if (NULL != msdInstance->msdCommand.transfer)
        {
            (void)USB_HostFreeTransfer(msdInstance->hostHandle, msdInstance->msdCommand.transfer);
        }
        (void)USB_HostCloseDeviceInterface(
            deviceHandle, msdInstance->interfaceHandle); /* notify host driver the interface is closed */
#if ((defined(USB_HOST_CONFIG_BUFFER_PROPERTY_CACHEABLE)) && (USB_HOST_CONFIG_BUFFER_PROPERTY_CACHEABLE > 0U))
        OSA_MemoryFreeAlign(msdInstance->msdCommand.cbwBlock);
        OSA_MemoryFreeAlign(msdInstance->msdCommand.cswBlock);
#else
        OSA_MemoryFree(msdInstance->msdCommand.cbwBlock);
        OSA_MemoryFree(msdInstance->msdCommand.cswBlock);
#endif
        OSA_MemoryFree(msdInstance);
    }
    else
    {
        (void)USB_HostCloseDeviceInterface(deviceHandle, NULL);
    }

    return kStatus_USB_Success;
}

static usb_status_t USB_HostMsdControl(usb_host_msd_instance_t *msdInstance,
                                       host_inner_transfer_callback_t pipeCallbackFn,
                                       transfer_callback_t callbackFn,
                                       void *callbackParam,
                                       uint8_t *buffer,
                                       uint16_t bufferLength,
                                       uint8_t requestType,
                                       uint8_t requestValue)
{
    usb_host_transfer_t *transfer;

    if (msdInstance == NULL)
    {
        return kStatus_USB_InvalidHandle;
    }

    /* malloc one transfer */
    if (USB_HostMallocTransfer(msdInstance->hostHandle, &transfer) != kStatus_USB_Success)
    {
#ifdef HOST_ECHO
        usb_echo("allocate transfer error\r\n");
#endif
        return kStatus_USB_Busy;
    }
    /* save the application callback function */
    msdInstance->controlCallbackFn    = callbackFn;
    msdInstance->controlCallbackParam = callbackParam;
    /* initialize transfer */
    transfer->transferBuffer = buffer;
    transfer->transferLength = bufferLength;
    transfer->callbackFn     = pipeCallbackFn;
    transfer->callbackParam  = msdInstance;

    transfer->setupPacket->bmRequestType = requestType;
    transfer->setupPacket->bRequest      = requestValue;
    transfer->setupPacket->wValue        = 0x0000;
    transfer->setupPacket->wIndex =
        ((usb_host_interface_t *)msdInstance->interfaceHandle)->interfaceDesc->bInterfaceNumber;
    transfer->setupPacket->wLength = bufferLength;

    if (USB_HostSendSetup(msdInstance->hostHandle, msdInstance->controlPipe, transfer) !=
        kStatus_USB_Success) /* call host driver api */
    {
        (void)USB_HostFreeTransfer(msdInstance->hostHandle, transfer);
        return kStatus_USB_Error;
    }
    msdInstance->controlTransfer = transfer;

    return kStatus_USB_Success;
}

usb_status_t USB_HostMsdMassStorageReset(usb_host_class_handle classHandle,
                                         transfer_callback_t callbackFn,
                                         void *callbackParam)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)classHandle;

    return USB_HostMsdControl(msdInstance, USB_HostMsdMassResetCallback, callbackFn, callbackParam, NULL, 0,
                              (USB_REQUEST_TYPE_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE),
                              USB_HOST_HID_MASS_STORAGE_RESET);
}

usb_status_t USB_HostMsdGetMaxLun(usb_host_class_handle classHandle,
                                  uint8_t *logicalUnitNumber,
                                  transfer_callback_t callbackFn,
                                  void *callbackParam)
{
    usb_host_msd_instance_t *msdInstance = (usb_host_msd_instance_t *)classHandle;

    return USB_HostMsdControl(
        msdInstance, USB_HostMsdControlCallback, callbackFn, callbackParam, logicalUnitNumber, 1,
        (USB_REQUEST_TYPE_DIR_IN | USB_REQUEST_TYPE_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE),
        USB_HOST_HID_GET_MAX_LUN);
}

#endif /* USB_HOST_CONFIG_MSD */
