#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

// CFG_TUSB_MCU is injected by the managed component's compile options (-DCFG_TUSB_MCU=OPT_MCU_ESP32S3)

#define CFG_TUSB_OS             OPT_OS_FREERTOS

// Port 0 = OTG peripheral (GPIO19=D-, GPIO20=D+)
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE

// Endpoint 0 max packet size
#define CFG_TUD_ENDPOINT0_SIZE  64

// CFG_TUD_ENABLED is derived automatically from CFG_TUSB_RHPORT0_MODE

// Enable MIDI class only
#define CFG_TUD_MIDI            1
#define CFG_TUD_MIDI_RX_BUFSIZE 64
#define CFG_TUD_MIDI_TX_BUFSIZE 64

// Disable unused classes
#define CFG_TUD_CDC     0
#define CFG_TUD_MSC     0
#define CFG_TUD_HID     0
#define CFG_TUD_VENDOR  0
#define CFG_TUD_AUDIO   0
#define CFG_TUD_VIDEO   0
#define CFG_TUD_DFU     0

#endif // TUSB_CONFIG_H
