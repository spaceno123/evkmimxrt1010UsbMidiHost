/*
 * host_midi.h
 *
 *  Created on: 2025/06/18
 *      Author: M.Akino
 */

#ifndef HOST_MIDI_H_
#define HOST_MIDI_H_

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief buffer for receiving data */
#define MIDI_BUFFER_SIZE (512U)

/*! @brief host midi run status */
typedef enum _usb_host_midi_run_state
{
    kUSB_HostMidiRunIdle = 0,         /*!< idle */
    kUSB_HostMidiRunSetInterface,     /*!< execute set interface code */
    kUSB_HostMidiRunWaitSetInterface, /*!< wait set interface done */
    kUSB_HostMidiRunSetInterfaceDone, /*!< set interface is done, execute next step */
    kUSB_HostMidiRunWaitDataReceived, /*!< wait interrupt in data */
    kUSB_HostMidiRunDataReceived,     /*!< interrupt in data received */
    kUSB_HostMidiRunPrimeDataReceive, /*!< prime interrupt in receive */
} usb_host_midi_run_state_t;

/*! @brief USB host midi instance structure */
typedef struct _host_midi_instance
{
    usb_host_configuration_handle configHandle; /*!< the midi's configuration handle */
    usb_device_handle deviceHandle;             /*!< the midi's device handle */
    usb_host_class_handle classHandle;          /*!< the midi's class handle */
    usb_host_interface_handle interfaceHandle;  /*!< the midi's interface handle */
    uint16_t bulkInMaxPacketSize;               /*!< bulk in max packet size */
    uint16_t bulkOutMaxPacketSize;              /*!< bulk out max packet size */
    uint8_t deviceState;                        /*!< device attach/detach status */
    uint8_t prevState;                          /*!< device attach/detach previous status */
    uint8_t runState;                           /*!< midi application run status */
    uint8_t runWaitState;                       /*!< midi application wait status, go to next run status when the wait status success */
    uint8_t *midiRxBuffer;                      /*!< use to receive data */
    uint16_t receiveCount;                      /*!< use to receive data count */
    uint8_t *midiTxBuffer;                      /*!< use to transfer data */
    uint8_t attachFlag;                         /*!< for send enable */
    uint8_t sendBusy;                           /*!< send busy */
} host_midi_instance_t;

/*******************************************************************************
 * API
 ******************************************************************************/

/*!
 * @brief host midi task function.
 *
 * This function implements the host midi action, it is used to create task.
 *
 * @param param   the host midi instance pointer.
 */
extern void USB_HostMidiTask(void *param);

/*!
 * @brief host midi callback function.
 *
 * This function should be called in the host callback function.
 *
 * @param deviceHandle         device handle.
 * @param configurationHandle  attached device's configuration descriptor information.
 * @param eventCode            callback event code, please reference to enumeration host_event_t.
 *
 * @retval kStatus_USB_Success              The host is initialized successfully.
 * @retval kStatus_USB_NotSupported         The configuration don't contain hid keyboard interface.
 */
extern usb_status_t USB_HostMidiEvent(usb_device_handle deviceHandle,
                                      usb_host_configuration_handle configurationHandle,
                                      uint32_t eventCode);

/*!
 * @brief host midi send function.
 *
 * This function sends a short MIDI packet.
 *
 * @param cn   cable number.
 * @param sts  midi message status.
 * @param dt1  midi message 1st data.
 * @param dt2  midi message 2nd data.(available)
 *
 * @retval true   successfully.
 * @retval false  buffer full.
 */
extern bool USB_HostMidiSendShortMessage(uint8_t cn, uint8_t sts, uint8_t dt1, uint8_t dt2);

#endif /* HOST_MIDI_H_ */
