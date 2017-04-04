/*
 * Copyright (c) 2009-2016 Vadim Mikhailov
 *
 * uhubctl main header file
 *
 * This file can be distributed under the terms and conditions of the
 * GNU General Public License version 2.
 */
#ifndef __UHUBCTL_H
#define __UHUBCTL_H

/* Max number of hub ports supported.
 * This is somewhat artifically limited by "-p" option parser.
 * If "-p" parser is improved, we can support up to 32 ports.
 * However, biggest number of ports on smart hub I've seen was 8.
 * I've also observed onboard USB hub with whopping 14 ports,
 * but that hub did not support per-port power switching.
 */
#define MAX_HUB_PORTS            9
#define ALL_HUB_PORTS            ((1 << MAX_HUB_PORTS) - 1) /* bitmask */

#define USB_CTRL_GET_TIMEOUT     5000

#define USB_PORT_FEAT_POWER      (1 << 3)

#define POWER_KEEP               (-1)
#define POWER_OFF                0
#define POWER_ON                 1
#define POWER_CYCLE              2

#define MAX_HUB_CHAIN            8  /* Per USB 3.0 spec max hub chain is 7 */

#define MAX_HUBS 64

/* Partially borrowed from linux/usb/ch11.h */
struct usb_hub_descriptor {
    unsigned char bDescLength;
    unsigned char bDescriptorType;
    unsigned char bNbrPorts;
    unsigned char wHubCharacteristics[2];
    unsigned char bPwrOn2PwrGood;
    unsigned char bHubContrCurrent;
    unsigned char data[1]; /* use 1 to avoid zero-sized array warning */
};

/*
 * Hub Status and Hub Change results
 * See USB 2.0 spec Table 11-19 and Table 11-20
 */
struct usb_port_status {
    int16_t wPortStatus;
    int16_t wPortChange;
} __attribute__ ((packed));

/*
 * wPortStatus bit field
 * See USB 2.0 spec Table 11-21
 */
#define USB_PORT_STAT_CONNECTION        0x0001
#define USB_PORT_STAT_ENABLE            0x0002
#define USB_PORT_STAT_SUSPEND           0x0004
#define USB_PORT_STAT_OVERCURRENT       0x0008
#define USB_PORT_STAT_RESET             0x0010
#define USB_PORT_STAT_L1                0x0020
/* bits 6 to 7 are reserved */
#define USB_PORT_STAT_POWER             0x0100
#define USB_PORT_STAT_LOW_SPEED         0x0200
#define USB_PORT_STAT_HIGH_SPEED        0x0400
#define USB_PORT_STAT_TEST              0x0800
#define USB_PORT_STAT_INDICATOR         0x1000
/* bits 13 to 15 are reserved */

/*
 * wHubCharacteristics (masks)
 * See USB 2.0 spec Table 11-13, offset 3
 */
#define HUB_CHAR_LPSM           0x0003 /* Logical Power Switching Mode mask */
#define HUB_CHAR_COMMON_LPSM    0x0000 /* All ports at once power switching */
#define HUB_CHAR_INDV_PORT_LPSM 0x0001 /* Per-port power switching */
#define HUB_CHAR_NO_LPSM        0x0002 /* No power switching */

#define HUB_CHAR_COMPOUND       0x0004 /* hub is part of a compound device */

#define HUB_CHAR_OCPM           0x0018 /* Over-Current Protection Mode mask */
#define HUB_CHAR_COMMON_OCPM    0x0000 /* All ports at once over-current protection */
#define HUB_CHAR_INDV_PORT_OCPM 0x0008 /* Per-port over-current protection */
#define HUB_CHAR_NO_OCPM        0x0010 /* No over-current protection support */

#define HUB_CHAR_TTTT           0x0060 /* TT Think Time mask */
#define HUB_CHAR_PORTIND        0x0080 /* per-port indicators (LEDs) */

/* List of all USB devices enumerated by libusb */
static struct libusb_device **usb_devs = NULL;

struct hub_info {
    struct libusb_device *dev;
    int nports;
    char vendor[16];
    char location[32];
};
#endif
