/*
 * usb_host_midi.h
 *
 *  Created on: 2025/06/17
 *      Author: M.Akino
 */

#ifndef _USB_HOST_MIDI_H_
#define _USB_HOST_MIDI_H_

/*******************************************************************************
 * MIDI class public structure, enumerations, macros, functions
 ******************************************************************************/

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*!
 * @addtogroup usb_host_midi_drv
 * @{
 */

/*! @brief MIDI class code */
#define USB_HOST_MIDI_CLASS_CODE (1U)
/*! @brief MIDI sub-class code */
#define USB_HOST_MIDI_SUBCLASS_CODE (3U)
/*! @brief MIDI protocol code */
#define USB_HOST_MIDI_PROTOCOL_CODE (0U)

/*! @brief MIDI instance structure and MIDI usb_host_class_handle pointer to this structure */
typedef struct _usb_host_midi_instance
{
    usb_host_handle hostHandle;                /*!< This instance's related host handle*/
    usb_device_handle deviceHandle;            /*!< This instance's related device handle*/
    usb_host_interface_handle interfaceHandle; /*!< This instance's related interface handle*/
    usb_host_pipe_handle controlPipe;          /*!< This instance's related device control pipe*/
    usb_host_pipe_handle inPipe;               /*!< MIDI bulk in pipe*/
    usb_host_pipe_handle outPipe;              /*!< MIDI bulk out pipe*/
    transfer_callback_t inCallbackFn;          /*!< MIDI bulk in transfer callback function pointer*/
    void *inCallbackParam;                     /*!< MIDI bulk in transfer callback parameter*/
    transfer_callback_t outCallbackFn;         /*!< MIDI bulk out transfer callback function pointer*/
    void *outCallbackParam;                    /*!< MIDI bulk out transfer callback parameter*/
    transfer_callback_t controlCallbackFn;     /*!< MIDI control transfer callback function pointer*/
    void *controlCallbackParam;                /*!< MIDI control transfer callback parameter*/
    usb_host_transfer_t *controlTransfer;      /*!< Ongoing control transfer*/
#if ((defined USB_HOST_CONFIG_CLASS_AUTO_CLEAR_STALL) && USB_HOST_CONFIG_CLASS_AUTO_CLEAR_STALL)
    uint8_t *stallDataBuffer; /*!< keep the data buffer for stall transfer's data*/
    uint32_t stallDataLength; /*!< keep the data length for stall transfer's data*/
#endif

    uint16_t inPacketSize;  /*!< MIDI bulk in maximum packet size*/
    uint16_t outPacketSize; /*!< MIDI bulk out maximum packet size*/
} usb_host_midi_instance_t;

/*******************************************************************************
 * API
 ******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @name USB host MIDI class APIs
 * @{
 */

/*!
 * @brief Initializes the MIDI instance.
 *
 * This function allocates the resources for the MIDI instance.
 *
 * @param[in] deviceHandle  The device handle.
 * @param[out] classHandle  Return class handle.
 *
 * @retval kStatus_USB_Success        The device is initialized successfully.
 * @retval kStatus_USB_AllocFail      Allocate memory fail.
 */
extern usb_status_t USB_HostMidiInit(usb_device_handle deviceHandle, usb_host_class_handle *classHandle);

/*!
 * @brief Sets the interface.
 *
 * This function binds the interface with the MIDI instance.
 *
 * @param[in] classHandle      The class handle.
 * @param[in] interfaceHandle  The interface handle.
 * @param[in] alternateSetting The alternate setting value.
 * @param[in] callbackFn       This callback is called after this function completes.
 * @param[in] callbackParam    The first parameter in the callback function.
 *
 * @retval kStatus_USB_Success        The device is initialized successfully.
 * @retval kStatus_USB_InvalidHandle  The classHandle is NULL pointer.
 * @retval kStatus_USB_Busy           There is no idle transfer.
 * @retval kStatus_USB_Error          Send transfer fail. See the USB_HostSendSetup.
 * @retval kStatus_USB_Busy           Callback return status, there is no idle pipe.
 * @retval kStatus_USB_TransferStall  Callback return status, the transfer is stalled by the device.
 * @retval kStatus_USB_Error          Callback return status, open pipe fail. See the USB_HostOpenPipe.
 */
extern usb_status_t USB_HostMidiSetInterface(usb_host_class_handle classHandle,
                                             usb_host_interface_handle interfaceHandle,
                                             uint8_t alternateSetting,
                                             transfer_callback_t callbackFn,
                                             void *callbackParam);

/*!
 * @brief Deinitializes the HID instance.
 *
 * This function frees the resources for the MIDI instance.
 *
 * @param[in] deviceHandle   The device handle.
 * @param[in] classHandle    The class handle.
 *
 * @retval kStatus_USB_Success        The device is de-initialized successfully.
 */
extern usb_status_t USB_HostMidiDeinit(usb_device_handle deviceHandle, usb_host_class_handle classHandle);

/*!
 * @brief Gets the pipe maximum packet size.
 *
 * @param[in] classHandle The class handle.
 * @param[in] pipeType    Its value is USB_ENDPOINT_CONTROL, USB_ENDPOINT_ISOCHRONOUS, USB_ENDPOINT_BULK or
 * USB_ENDPOINT_INTERRUPT.
 *                        See the usb_spec.h
 * @param[in] direction   Pipe direction.
 *
 * @retval 0        The classHandle is NULL.
 * @retval          Maximum packet size.
 */
extern uint16_t USB_HostMidiGetPacketsize(usb_host_class_handle classHandle, uint8_t pipeType, uint8_t direction);

/*!
 * @brief Receives data.
 *
 * This function implements the MIDI receiving data.
 *
 * @param[in] classHandle   The class handle.
 * @param[out] buffer       The buffer pointer.
 * @param[in] bufferLength  The buffer length.
 * @param[in] callbackFn    This callback is called after this function completes.
 * @param[in] callbackParam The first parameter in the callback function.
 *
 * @retval kStatus_USB_Success        Receive request successfully.
 * @retval kStatus_USB_InvalidHandle  The classHandle is NULL pointer.
 * @retval kStatus_USB_Busy           There is no idle transfer.
 * @retval kStatus_USB_Error          Pipe is not initialized.
 *                                    Or, send transfer fail. See the USB_HostRecv.
 */
extern usb_status_t USB_HostMidiRecv(usb_host_class_handle classHandle,
                                     uint8_t *buffer,
                                     uint32_t bufferLength,
                                     transfer_callback_t callbackFn,
                                     void *callbackParam);

/*!
 * @brief Sends data.
 *
 * This function implements the MIDI sending data.
 *
 * @param[in] classHandle   The class handle.
 * @param[in] buffer        The buffer pointer.
 * @param[in] bufferLength  The buffer length.
 * @param[in] callbackFn    This callback is called after this function completes.
 * @param[in] callbackParam The first parameter in the callback function.
 *
 * @retval kStatus_USB_Success        Send request successfully.
 * @retval kStatus_USB_InvalidHandle  The classHandle is NULL pointer.
 * @retval kStatus_USB_Busy           There is no idle transfer.
 * @retval kStatus_USB_Error          Pipe is not initialized.
 *                                    Or, send transfer fail. See the USB_HostSend.
 */
extern usb_status_t USB_HostMidiSend(usb_host_class_handle classHandle,
                                     uint8_t *buffer,
                                     uint32_t bufferLength,
                                     transfer_callback_t callbackFn,
                                     void *callbackParam);

/*! @}*/

#ifdef __cplusplus
}
#endif

/*! @}*/

#endif /* _USB_HOST_MIDI_H_ */
