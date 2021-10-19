/*
 * Copyright (c) 2009-2020 Vadim Mikhailov
 *
 * Utility to turn USB port power on/off
 * for USB hubs that support per-port power switching.
 *
 * This file can be distributed under the terms and conditions of the
 * GNU General Public License version 2.
 *
 */

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <process.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(_WIN32)
#include <libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) /* snprintf is not available in pure C mode */
int snprintf(char * __restrict __str, size_t __size, const char * __restrict __format, ...) __printflike(3, 4);
#endif

#if !defined(LIBUSB_API_VERSION) || (LIBUSB_API_VERSION <= 0x01000103)
#define LIBUSB_DT_SUPERSPEED_HUB 0x2a
#endif

#if _POSIX_C_SOURCE >= 199309L
#include <time.h>   /* for nanosleep */
#endif

/* cross-platform sleep function */

void sleep_ms(int milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    usleep(milliseconds * 1000);
#endif
}

/* Max number of hub ports supported */
#define MAX_HUB_PORTS            14
#define ALL_HUB_PORTS            ((1 << MAX_HUB_PORTS) - 1) /* bitmask */

#define USB_CTRL_GET_TIMEOUT     5000

#define USB_PORT_FEAT_POWER      (1 << 3)

#define POWER_KEEP               (-1)
#define POWER_OFF                0
#define POWER_ON                 1
#define POWER_CYCLE              2
#define POWER_TOGGLE             3

#define MAX_HUB_CHAIN            8  /* Per USB 3.0 spec max hub chain is 7 */

/* Partially borrowed from linux/usb/ch11.h */

#pragma pack(push,1)
struct usb_hub_descriptor {
    unsigned char bDescLength;
    unsigned char bDescriptorType;
    unsigned char bNbrPorts;
    unsigned char wHubCharacteristics[2];
    unsigned char bPwrOn2PwrGood;
    unsigned char bHubContrCurrent;
    unsigned char data[1]; /* use 1 to avoid zero-sized array warning */
};
#pragma pack(pop)

/*
 * Hub Status and Hub Change results
 * See USB 2.0 spec Table 11-19 and Table 11-20
 */
#pragma pack(push,1)
struct usb_port_status {
    int16_t wPortStatus;
    int16_t wPortChange;
};
#pragma pack(pop)

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


#define USB_SS_BCD                      0x0300
/*
 * Additions to wPortStatus bit field from USB 3.0
 * See USB 3.0 spec Table 10-10
 */
#define USB_PORT_STAT_LINK_STATE        0x01e0
#define USB_SS_PORT_STAT_POWER          0x0200
#define USB_SS_PORT_STAT_SPEED          0x1c00
#define USB_PORT_STAT_SPEED_5GBPS       0x0000
/* Valid only if port is enabled */
/* Bits that are the same from USB 2.0 */
#define USB_SS_PORT_STAT_MASK (USB_PORT_STAT_CONNECTION  | \
                               USB_PORT_STAT_ENABLE      | \
                               USB_PORT_STAT_OVERCURRENT | \
                               USB_PORT_STAT_RESET)

/*
 * Definitions for PORT_LINK_STATE values
 * (bits 5-8) in wPortStatus
 */
#define USB_SS_PORT_LS_U0               0x0000
#define USB_SS_PORT_LS_U1               0x0020
#define USB_SS_PORT_LS_U2               0x0040
#define USB_SS_PORT_LS_U3               0x0060
#define USB_SS_PORT_LS_SS_DISABLED      0x0080
#define USB_SS_PORT_LS_RX_DETECT        0x00a0
#define USB_SS_PORT_LS_SS_INACTIVE      0x00c0
#define USB_SS_PORT_LS_POLLING          0x00e0
#define USB_SS_PORT_LS_RECOVERY         0x0100
#define USB_SS_PORT_LS_HOT_RESET        0x0120
#define USB_SS_PORT_LS_COMP_MOD         0x0140
#define USB_SS_PORT_LS_LOOPBACK         0x0160


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

struct descriptor_strings {
    char vendor[64];
    char product[64];
    char serial[64];
    char description[512];
};

struct hub_info {
    struct libusb_device *dev;
    int bcd_usb;
    int super_speed; /* 1 if super speed hub, and 0 otherwise */
    int nports;
    int lpsm; /* logical power switching mode */
    int actionable; /* true if this hub is subject to action */
    char container_id[33]; /* container ID as hex string */
    char vendor[16];
    char location[32];
    uint8_t bus;
    uint8_t port_numbers[MAX_HUB_CHAIN];
    int pn_len; /* length of port numbers */
    struct descriptor_strings ds;
};

/* Array of all enumerated USB hubs */
#define MAX_HUBS 128
static struct hub_info hubs[MAX_HUBS];
static int hub_count = 0;
static int hub_phys_count = 0;

/* default options */
static char opt_vendor[16]   = "";
static char opt_search[64]   = "";     /* Search by attached device description */
static char opt_location[32] = "";     /* Hub location a-b.c.d */
static int opt_level = 0;              /* Hub location level (e.g., a-b is level 2, a-b.c is level 3)*/
static int opt_ports  = ALL_HUB_PORTS; /* Bitmask of ports to operate on */
static int opt_action = POWER_KEEP;
static double opt_delay = 2;
static int opt_repeat = 1;
static int opt_wait   = 20; /* wait before repeating in ms */
static int opt_exact  = 0;  /* exact location match - disable USB3 duality handling */
static int opt_reset  = 0;  /* reset hub after operation(s) */
static int opt_force  = 0;  /* force operation even on unsupported hubs */
static int opt_nodesc = 0;  /* skip querying device description */

static const struct option long_options[] = {
    { "location", required_argument, NULL, 'l' },
    { "vendor",   required_argument, NULL, 'n' },
    { "search",   required_argument, NULL, 's' },
    { "level",    required_argument, NULL, 'L' },
    { "ports",    required_argument, NULL, 'p' },
    { "action",   required_argument, NULL, 'a' },
    { "delay",    required_argument, NULL, 'd' },
    { "repeat",   required_argument, NULL, 'r' },
    { "wait",     required_argument, NULL, 'w' },
    { "exact",    no_argument,       NULL, 'e' },
    { "force",    no_argument,       NULL, 'f' },
    { "nodesc",   no_argument,       NULL, 'N' },
    { "reset",    no_argument,       NULL, 'R' },
    { "version",  no_argument,       NULL, 'v' },
    { "help",     no_argument,       NULL, 'h' },
    { 0,          0,                 NULL, 0   },
};


static int print_usage()
{
    printf(
        "uhubctl: utility to control USB port power for smart hubs.\n"
        "Usage: uhubctl [options]\n"
        "Without options, show status for all smart hubs.\n"
        "\n"
        "Options [defaults in brackets]:\n"
        "--action,   -a - action to off/on/cycle/toggle (0/1/2/3) for affected ports.\n"
        "--ports,    -p - ports to operate on    [all hub ports].\n"
        "--location, -l - limit hub by location  [all smart hubs].\n"
        "--level     -L - limit hub by location level (e.g. a-b.c is level 3).\n"
        "--vendor,   -n - limit hub by vendor id [%s] (partial ok).\n"
        "--search,   -s - limit hub by attached device description.\n"
        "--delay,    -d - delay for cycle action [%g sec].\n"
        "--repeat,   -r - repeat power off count [%d] (some devices need it to turn off).\n"
        "--exact,    -e - exact location (no USB3 duality handling).\n"
        "--force,    -f - force operation even on unsupported hubs.\n"
        "--nodesc,   -N - do not query device description (helpful for unresponsive devices).\n"
        "--reset,    -R - reset hub after each power-on action, causing all devices to reassociate.\n"
        "--wait,     -w - wait before repeat power off [%d ms].\n"
        "--version,  -v - print program version.\n"
        "--help,     -h - print this text.\n"
        "\n"
        "Send bugs and requests to: https://github.com/mvp/uhubctl\n"
        "version: %s\n",
        strlen(opt_vendor) ? opt_vendor : "any",
        opt_delay,
        opt_repeat,
        opt_wait,
        PROGRAM_VERSION
    );
    return 0;
}


/* trim trailing spaces from a string */

static char* rtrim(char* str)
{
    int i;
    for (i = strlen(str)-1; i>=0 && isspace(str[i]); i--) {
        str[i] = 0;
    }
    return str;
}

/*
 * Convert port list into bitmap.
 * Following port list specifications are equivalent:
 *   1,3,4,5,11,12,13
 *   1,3-5,11-13
 * Returns: bitmap of specified ports, max port is MAX_HUB_PORTS.
 */

static int ports2bitmap(char* const portlist)
{
    int ports = 0;
    char* position = portlist;
    char* comma;
    char* dash;
    int len;
    int i;
    while (position) {
        char buf[8] = {0};
        comma = strchr(position, ',');
        len = sizeof(buf) - 1;
        if (comma) {
            if (len > comma - position)
                len = comma - position;
            strncpy(buf, position, len);
            position = comma + 1;
        } else {
            strncpy(buf, position, len);
            position = NULL;
        }
        /* Check if we have port range, e.g.: a-b */
        int a=0, b=0;
        a = atoi(buf);
        dash = strchr(buf, '-');
        if (dash) {
            b = atoi(dash+1);
        } else {
            b = a;
        }
        if (a > b) {
            fprintf(stderr, "Bad port spec %d-%d, first port must be less than last\n", a, b);
            exit(1);
        }
        if (a <= 0 || a > MAX_HUB_PORTS || b <= 0 || b > MAX_HUB_PORTS) {
            fprintf(stderr, "Bad port spec %d-%d, port numbers must be from 1 to %d\n", a, b, MAX_HUB_PORTS);
            exit(1);
        }
        for (i=a; i<=b; i++) {
            ports |= (1 << (i-1));
        }
    }
    return ports;
}


/*
 * Compatibility wrapper around libusb_get_port_numbers()
 */

static int get_port_numbers(libusb_device *dev, uint8_t *buf, uint8_t bufsize)
{
    int pcount;
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000102)
    /*
     * libusb_get_port_path is deprecated since libusb v1.0.16,
     * therefore use libusb_get_port_numbers when supported
     */
    pcount = libusb_get_port_numbers(dev, buf, bufsize);
#else
    pcount = libusb_get_port_path(NULL, dev, buf, bufsize);
#endif
    return pcount;
}

/*
 * get USB hub properties.
 * most hub_info fields are filled, except for description.
 * returns 0 for success and error code for failure.
 */

static int get_hub_info(struct libusb_device *dev, struct hub_info *info)
{
    int rc = 0;
    int len = 0;
    struct libusb_device_handle *devh = NULL;
    unsigned char buf[LIBUSB_DT_HUB_NONVAR_SIZE + 2 + 3] = {0};
    struct usb_hub_descriptor *uhd = (struct usb_hub_descriptor *)buf;
    int minlen = LIBUSB_DT_HUB_NONVAR_SIZE + 2;
    struct libusb_device_descriptor desc;
    rc = libusb_get_device_descriptor(dev, &desc);
    if (rc)
        return rc;
    if (desc.bDeviceClass != LIBUSB_CLASS_HUB)
        return LIBUSB_ERROR_INVALID_PARAM;
    int bcd_usb = libusb_le16_to_cpu(desc.bcdUSB);
    int desc_type = bcd_usb >= USB_SS_BCD ? LIBUSB_DT_SUPERSPEED_HUB
                                          : LIBUSB_DT_HUB;
    rc = libusb_open(dev, &devh);
    if (rc == 0) {
        len = libusb_control_transfer(devh,
            LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS
                               | LIBUSB_RECIPIENT_DEVICE, /* hub status */
            LIBUSB_REQUEST_GET_DESCRIPTOR,
            desc_type << 8,
            0,
            buf, sizeof(buf),
            USB_CTRL_GET_TIMEOUT
        );

        if (len >= minlen) {
            info->dev     = dev;
            info->bcd_usb = bcd_usb;
            info->super_speed = (bcd_usb >= USB_SS_BCD);
            info->nports  = uhd->bNbrPorts;
            snprintf(
                info->vendor, sizeof(info->vendor),
                "%04x:%04x",
                libusb_le16_to_cpu(desc.idVendor),
                libusb_le16_to_cpu(desc.idProduct)
            );

            /* Convert bus and ports array into USB location string */
            info->bus = libusb_get_bus_number(dev);
            snprintf(info->location, sizeof(info->location), "%d", info->bus);
            info->pn_len = get_port_numbers(dev, info->port_numbers, sizeof(info->port_numbers));
            int k;
            for (k=0; k < info->pn_len; k++) {
                char s[8];
                snprintf(s, sizeof(s), "%s%d", k==0 ? "-" : ".", info->port_numbers[k]);
                strcat(info->location, s);
            }

            /* Get container_id: */
            bzero(info->container_id, sizeof(info->container_id));
            struct libusb_bos_descriptor *bos;
            rc = libusb_get_bos_descriptor(devh, &bos);
            if (rc == 0) {
                int cap;
#ifdef __FreeBSD__
                for (cap=0; cap < bos->bNumDeviceCapabilities; cap++) {
#else
                for (cap=0; cap < bos->bNumDeviceCaps; cap++) {
#endif
                    if (bos->dev_capability[cap]->bDevCapabilityType == LIBUSB_BT_CONTAINER_ID) {
                        struct libusb_container_id_descriptor *container_id;
                        rc = libusb_get_container_id_descriptor(NULL, bos->dev_capability[cap], &container_id);
                        if (rc == 0) {
                            int i;
                            for (i=0; i<16; i++) {
                                sprintf(info->container_id+i*2, "%02x", container_id->ContainerID[i]);
                            }
                            info->container_id[i*2] = 0;
                            libusb_free_container_id_descriptor(container_id);
                        }
                    }
                }
                libusb_free_bos_descriptor(bos);

                /* Raspberry Pi 4B hack for USB3 root hub: */
                if (strlen(info->container_id)==0 &&
                    strcasecmp(info->vendor, "1d6b:0003")==0 &&
                    info->pn_len==0 &&
                    info->nports==4 &&
                    bcd_usb==USB_SS_BCD)
                {
                    strcpy(info->container_id, "5cf3ee30d5074925b001802d79434c30");
                }
            }

            /* Logical Power Switching Mode */
            int lpsm = uhd->wHubCharacteristics[0] & HUB_CHAR_LPSM;
            if (lpsm == HUB_CHAR_COMMON_LPSM && info->nports == 1) {
                /* For 1 port hubs, ganged power switching is the same as per-port: */
                lpsm = HUB_CHAR_INDV_PORT_LPSM;
            }
            /* Raspberry Pi 4B reports inconsistent descriptors, override: */
            if (lpsm == HUB_CHAR_COMMON_LPSM && strcasecmp(info->vendor, "2109:3431")==0) {
                lpsm = HUB_CHAR_INDV_PORT_LPSM;
            }
            info->lpsm = lpsm;
            rc = 0;
        } else {
            rc = len;
        }
        libusb_close(devh);
    }
    return rc;
}


/*
 * Assuming that devh is opened device handle for USB hub,
 * return state for given hub port.
 * In case of error, returns -1 (inspect errno for more information).
 */

static int get_port_status(struct libusb_device_handle *devh, int port)
{
    int rc;
    struct usb_port_status ust;
    if (devh == NULL)
        return -1;

    rc = libusb_control_transfer(devh,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS
                           | LIBUSB_RECIPIENT_OTHER, /* port status */
        LIBUSB_REQUEST_GET_STATUS, 0,
        port, (unsigned char*)&ust, sizeof(ust),
        USB_CTRL_GET_TIMEOUT
    );

    if (rc < 0) {
        return rc;
    }
    return ust.wPortStatus;
}


/*
 * Get USB device descriptor strings and summary description.
 *
 * Summary will use following format:
 *
 *    "<vid:pid> <vendor> <product> <serial>, <USB x.yz, N ports>"
 *
 * vid:pid will be always present, but vendor, product or serial
 * may be skipped if they are empty or not enough permissions to read them.
 * <USB x.yz, N ports> will be present only for USB hubs.
 *
 * Returns 0 for success and error code for failure.
 * In case of failure return buffer is not altered.
 */

static int get_device_description(struct libusb_device * dev, struct descriptor_strings * ds)
{
    int rc;
    int id_vendor  = 0;
    int id_product = 0;
    char hub_specific[64] = "";
    struct libusb_device_descriptor desc;
    struct libusb_device_handle *devh = NULL;
    rc = libusb_get_device_descriptor(dev, &desc);
    if (rc)
        return rc;
    bzero(ds, sizeof(*ds));
    id_vendor  = libusb_le16_to_cpu(desc.idVendor);
    id_product = libusb_le16_to_cpu(desc.idProduct);
    rc = libusb_open(dev, &devh);
    if (rc == 0) {
        if (!opt_nodesc) {
            if (desc.iManufacturer) {
                rc = libusb_get_string_descriptor_ascii(devh,
                    desc.iManufacturer, (unsigned char*)ds->vendor, sizeof(ds->vendor));
                rtrim(ds->vendor);
            }
            if (rc >= 0 && desc.iProduct) {
                rc = libusb_get_string_descriptor_ascii(devh,
                    desc.iProduct, (unsigned char*)ds->product, sizeof(ds->product));
                rtrim(ds->product);
            }
            if (rc >= 0 && desc.iSerialNumber) {
                rc = libusb_get_string_descriptor_ascii(devh,
                    desc.iSerialNumber, (unsigned char*)ds->serial, sizeof(ds->serial));
                rtrim(ds->serial);
            }
        }
        if (desc.bDeviceClass == LIBUSB_CLASS_HUB) {
            struct hub_info info;
            rc = get_hub_info(dev, &info);
            if (rc == 0) {
                const char * lpsm_type;
                if (info.lpsm == HUB_CHAR_INDV_PORT_LPSM) {
                    lpsm_type = "ppps";
                } else if (info.lpsm == HUB_CHAR_COMMON_LPSM) {
                    lpsm_type = "ganged";
                } else {
                    lpsm_type = "nops";
                }
                snprintf(hub_specific, sizeof(hub_specific), ", USB %x.%02x, %d ports, %s",
                   info.bcd_usb >> 8, info.bcd_usb & 0xFF, info.nports, lpsm_type);
            }
        }
        libusb_close(devh);
    }
    snprintf(ds->description, sizeof(ds->description),
        "%04x:%04x%s%s%s%s%s%s%s",
        id_vendor, id_product,
        ds->vendor[0]  ? " " : "", ds->vendor,
        ds->product[0] ? " " : "", ds->product,
        ds->serial[0]  ? " " : "", ds->serial,
        hub_specific
    );
    return 0;
}


/*
 * show status for hub ports
 * portmask is bitmap of ports to display
 * if portmask is 0, show all ports
 */

static int print_port_status(struct hub_info * hub, int portmask)
{
    int port_status;
    struct libusb_device_handle * devh = NULL;
    int rc = 0;
    struct libusb_device *dev = hub->dev;
    rc = libusb_open(dev, &devh);
    if (rc == 0) {
        int port;
        for (port = 1; port <= hub->nports; port++) {
            if (portmask > 0 && (portmask & (1 << (port-1))) == 0) continue;

            port_status = get_port_status(devh, port);
            if (port_status == -1) {
                fprintf(stderr,
                    "cannot read port %d status, %s (%d)\n",
                    port, strerror(errno), errno);
                break;
            }

            printf("  Port %d: %04x", port, port_status);

            struct descriptor_strings ds;
            bzero(&ds, sizeof(ds));
            struct libusb_device * udev;
            int i = 0;
            while ((udev = usb_devs[i++]) != NULL) {
                uint8_t dev_bus;
                uint8_t dev_pn[MAX_HUB_CHAIN];
                int dev_plen;
                dev_bus = libusb_get_bus_number(udev);
                /* only match devices on the same bus: */
                if (dev_bus != hub->bus) continue;
                dev_plen = get_port_numbers(udev, dev_pn, sizeof(dev_pn));
                if ((dev_plen == hub->pn_len + 1) &&
                    (memcmp(hub->port_numbers, dev_pn, hub->pn_len) == 0) &&
                    libusb_get_port_number(udev) == port)
                {
                    rc = get_device_description(udev, &ds);
                    if (rc == 0)
                        break;
                }
            }

            if (!hub->super_speed) {
                if (port_status == 0) {
                    printf(" off");
                } else {
                    if (port_status & USB_PORT_STAT_POWER)        printf(" power");
                    if (port_status & USB_PORT_STAT_INDICATOR)    printf(" indicator");
                    if (port_status & USB_PORT_STAT_TEST)         printf(" test");
                    if (port_status & USB_PORT_STAT_HIGH_SPEED)   printf(" highspeed");
                    if (port_status & USB_PORT_STAT_LOW_SPEED)    printf(" lowspeed");
                    if (port_status & USB_PORT_STAT_SUSPEND)      printf(" suspend");
                }
            } else {
                if (!(port_status & USB_SS_PORT_STAT_POWER)) {
                    printf(" off");
                } else {
                    int link_state = port_status & USB_PORT_STAT_LINK_STATE;
                    if (port_status & USB_SS_PORT_STAT_POWER)     printf(" power");
                    if ((port_status & USB_SS_PORT_STAT_SPEED)
                         == USB_PORT_STAT_SPEED_5GBPS)
                    {
                        printf(" 5gbps");
                    }
                    if (link_state == USB_SS_PORT_LS_U0)          printf(" U0");
                    if (link_state == USB_SS_PORT_LS_U1)          printf(" U1");
                    if (link_state == USB_SS_PORT_LS_U2)          printf(" U2");
                    if (link_state == USB_SS_PORT_LS_U3)          printf(" U3");
                    if (link_state == USB_SS_PORT_LS_SS_DISABLED) printf(" SS.Disabled");
                    if (link_state == USB_SS_PORT_LS_RX_DETECT)   printf(" Rx.Detect");
                    if (link_state == USB_SS_PORT_LS_SS_INACTIVE) printf(" SS.Inactive");
                    if (link_state == USB_SS_PORT_LS_POLLING)     printf(" Polling");
                    if (link_state == USB_SS_PORT_LS_RECOVERY)    printf(" Recovery");
                    if (link_state == USB_SS_PORT_LS_HOT_RESET)   printf(" HotReset");
                    if (link_state == USB_SS_PORT_LS_COMP_MOD)    printf(" Compliance");
                    if (link_state == USB_SS_PORT_LS_LOOPBACK)    printf(" Loopback");
                }
            }
            if (port_status & USB_PORT_STAT_RESET)       printf(" reset");
            if (port_status & USB_PORT_STAT_OVERCURRENT) printf(" oc");
            if (port_status & USB_PORT_STAT_ENABLE)      printf(" enable");
            if (port_status & USB_PORT_STAT_CONNECTION)  printf(" connect");

            if (port_status & USB_PORT_STAT_CONNECTION)  printf(" [%s]", ds.description);

            printf("\n");
        }
        libusb_close(devh);
    }
    return 0;
}


/*
 *  Find all USB hubs and fill hubs[] array.
 *  Set actionable to 1 on all hubs that we are going to operate on
 *  (this applies possible constraints like location or vendor).
 *  Returns count of found actionable physical hubs
 *  (USB3 hubs are counted once despite having USB2 dual partner).
 *  In case of error returns negative error code.
 */

static int usb_find_hubs()
{
    struct libusb_device *dev;
    int perm_ok = 1;
    int rc = 0;
    int i = 0;
    int j = 0;
    while ((dev = usb_devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        rc = libusb_get_device_descriptor(dev, &desc);
        /* only scan for hubs: */
        if (rc == 0 && desc.bDeviceClass != LIBUSB_CLASS_HUB)
            continue;
        struct hub_info info;
        bzero(&info, sizeof(info));
        rc = get_hub_info(dev, &info);
        if (rc) {
            perm_ok = 0; /* USB permission issue? */
            continue;
        }
        get_device_description(dev, &info.ds);
        if (info.lpsm != HUB_CHAR_INDV_PORT_LPSM && !opt_force) {
            continue;
        }
        info.actionable = 1;
        if (strlen(opt_search) > 0) {
            /* Search by attached device description */
            info.actionable = 0;
            struct libusb_device * udev;
            int k = 0;
            while ((udev = usb_devs[k++]) != NULL) {
                uint8_t dev_pn[MAX_HUB_CHAIN];
                uint8_t dev_bus = libusb_get_bus_number(udev);
                /* only match devices on the same bus: */
                if (dev_bus != info.bus) continue;
                int dev_plen = get_port_numbers(udev, dev_pn, sizeof(dev_pn));
                if ((dev_plen == info.pn_len + 1) &&
                    (memcmp(info.port_numbers, dev_pn, info.pn_len) == 0))
                {
                    struct descriptor_strings ds;
                    bzero(&ds, sizeof(ds));
                    rc = get_device_description(udev, &ds);
                    if (rc != 0)
                        break;
                    if (strstr(ds.description, opt_search)) {
                        info.actionable = 1;
                        opt_ports &= 1 << (dev_pn[dev_plen-1] - 1);
                        break;
                    }
                }
            }
        }
        if (strlen(opt_location) > 0) {
            if (strcasecmp(opt_location, info.location)) {
                info.actionable = 0;
            }
        }
        if (opt_level > 0) {
            if (opt_level != info.pn_len + 1) {
                info.actionable = 0;
            }
        }
        if (strlen(opt_vendor) > 0) {
            if (strncasecmp(opt_vendor, info.vendor, strlen(opt_vendor))) {
                info.actionable = 0;
            }
        }
        memcpy(&hubs[hub_count], &info, sizeof(info));
        if (hub_count < MAX_HUBS) {
            hub_count++;
        } else {
            /* That should be impossible - but we don't want to crash! */
            fprintf(stderr, "Too many hubs!");
            break;
        }
    }
    if (!opt_exact) {
        /* Handle USB2/3 duality: */
        for (i=0; i<hub_count; i++) {
            /* Check only actionable hubs: */
            if (hubs[i].actionable != 1)
                continue;
            /* Must have non empty container ID: */
            if (strlen(hubs[i].container_id) == 0)
                continue;
            int best_match = -1;
            int best_score = -1;
            for (j=0; j<hub_count; j++) {
                if (i==j)
                    continue;

                /* Find hub which is USB2/3 dual to the hub above */

                /* Hub and its dual must be different types: one USB2, another USB3: */
                if (hubs[i].super_speed == hubs[j].super_speed)
                    continue;

                /* Must have non empty container ID: */
                if (strlen(hubs[j].container_id) == 0)
                    continue;

                /* Per USB 3.0 spec chapter 11.2, container IDs must match: */
                if (strcmp(hubs[i].container_id, hubs[j].container_id) != 0)
                    continue;

                /* At this point, it should be enough to claim a match.
                 * However, some devices use hardcoded non-unique container ID.
                 * We should do few more checks below if multiple such devices are present.
                 */

                /* Hubs should have the same number of ports */
                if (hubs[i].nports != hubs[j].nports) {
                    /* Except for some weird hubs like Apple mini-dock (has 2 usb2 + 1 usb3 ports) */
                    if (hubs[i].nports + hubs[j].nports > 3) {
                        continue;
                    }
                }

                /* If serial numbers are both present, they must match: */
                if ((strlen(hubs[i].ds.serial) > 0 && strlen(hubs[j].ds.serial) > 0) &&
                    strcmp(hubs[i].ds.serial, hubs[j].ds.serial) != 0)
                {
                    continue;
                }

                /* We have first possible candidate, but need to keep looking for better one */

                if (best_score < 1) {
                    best_score = 1;
                    best_match = j;
                }

                /* Checks for various levels of USB2 vs USB3 path similarity... */

                uint8_t* p1 = hubs[i].port_numbers;
                uint8_t* p2 = hubs[j].port_numbers;
                int l1 = hubs[i].pn_len;
                int l2 = hubs[j].pn_len;
                int s1 = hubs[i].super_speed;
                int s2 = hubs[j].super_speed;

                /* Check if port path is the same after removing top level (needed for M1 Macs): */
                if (l1 >= 1 && l1 == l2 && memcmp(p1 + 1, p2 + 1, l1 - 1)==0) {
                    if (best_score < 2) {
                        best_score = 2;
                        best_match = j;
                    }
                }

                /* Raspberry Pi 4B hack (USB2 hub is one level deeper than USB3): */
                if (l1 + s1 == l2 + s2 && l1 >= s2 && memcmp(p1 + s2, p2 + s1, l1 - s2)==0) {
                    if (best_score < 3) {
                        best_score = 3;
                        best_match = j;
                    }
                }
                /* Check if port path is exactly the same: */
                if (l1 == l2 && memcmp(p1, p2, l1)==0) {
                    if (best_score < 4) {
                        best_score = 4;
                        best_match = j;
                    }
                    /* Give even higher priority if `usb2bus + 1 == usb3bus` (Linux specific): */
                    if (hubs[i].bus - s1 == hubs[j].bus - s2) {
                        if (best_score < 5) {
                            best_score = 5;
                            best_match = j;
                        }
                    }
                }
            }
            if (best_match >= 0) {
                if (!hubs[best_match].actionable) {
                    /* Use 2 to signify that this is derived dual device */
                    hubs[best_match].actionable = 2;
                }
            }
        }
    }
    hub_phys_count = 0;
    for (i=0; i<hub_count; i++) {
        if (!hubs[i].actionable)
            continue;
        if (!hubs[i].super_speed || opt_exact) {
            hub_phys_count++;
        }
    }
    if (perm_ok == 0 && hub_phys_count == 0) {
#ifdef __gnu_linux__
        if (geteuid() != 0) {
            fprintf(stderr,
                "There were permission problems while accessing USB.\n"
                "Follow https://git.io/JIB2Z for a fix!\n"
            );
        }
#endif
        return LIBUSB_ERROR_ACCESS;
    }
    return hub_phys_count;
}


int main(int argc, char *argv[])
{
    int rc;
    int c = 0;
    int option_index = 0;

    for (;;) {
        c = getopt_long(argc, argv, "l:L:n:a:p:d:r:w:s:hvefRN",
            long_options, &option_index);
        if (c == -1)
            break;  /* no more options left */
        switch (c) {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[option_index].flag != 0)
                break;
            printf("option %s", long_options[option_index].name);
            if (optarg)
                printf(" with arg %s", optarg);
            printf("\n");
            break;
        case 'l':
            strncpy(opt_location, optarg, sizeof(opt_location));
            break;
        case 'L':
            opt_level = atoi(optarg);
            break;
        case 'n':
            strncpy(opt_vendor, optarg, sizeof(opt_vendor));
            break;
        case 's':
            strncpy(opt_search, optarg, sizeof(opt_search));
            break;
        case 'p':
            if (!strcasecmp(optarg, "all")) { /* all ports is the default */
                break;
            }
            if (strlen(optarg)) {
                opt_ports = ports2bitmap(optarg);
            }
            break;
        case 'a':
            if (!strcasecmp(optarg, "off")   || !strcasecmp(optarg, "0")) {
                opt_action = POWER_OFF;
            }
            if (!strcasecmp(optarg, "on")    || !strcasecmp(optarg, "1")) {
                opt_action = POWER_ON;
            }
            if (!strcasecmp(optarg, "cycle") || !strcasecmp(optarg, "2")) {
                opt_action = POWER_CYCLE;
            }
            if (!strcasecmp(optarg, "toggle") || !strcasecmp(optarg, "3")) {
                opt_action = POWER_TOGGLE;
            }
            break;
        case 'd':
            opt_delay = atof(optarg);
            break;
        case 'r':
            opt_repeat = atoi(optarg);
            break;
        case 'f':
            opt_force = 1;
            break;
        case 'N':
            opt_nodesc = 1;
            break;
        case 'e':
            opt_exact = 1;
            break;
        case 'R':
            opt_reset = 1;
            break;
        case 'w':
            opt_wait = atoi(optarg);
            break;
        case 'v':
            printf("%s\n", PROGRAM_VERSION);
            exit(0);
            break;
        case 'h':
            print_usage();
            exit(1);
            break;
        case '?':
            /* getopt_long has already printed an error message here */
            fprintf(stderr, "Run with -h to get usage info.\n");
            exit(1);
            break;
        default:
            abort();
        }
    }
    if (optind < argc) {
        /* non-option parameters are found? */
        fprintf(stderr, "Invalid command line syntax!\n");
        fprintf(stderr, "Run with -h to get usage info.\n");
        exit(1);
    }

    rc = libusb_init(NULL);
    if (rc < 0) {
        fprintf(stderr,
            "Error initializing USB!\n"
        );
        exit(1);
    }

    rc = libusb_get_device_list(NULL, &usb_devs);
    if (rc < 0) {
        fprintf(stderr,
            "Cannot enumerate USB devices!\n"
        );
        rc = 1;
        goto cleanup;
    }

    rc = usb_find_hubs();
    if (rc <= 0) {
        fprintf(stderr,
            "No compatible devices detected%s%s!\n"
            "Run with -h to get usage info.\n",
            strlen(opt_location) ? " at location " : "",
            opt_location
        );
        rc = 1;
        goto cleanup;
    }

    if (hub_phys_count > 1 && opt_action >= 0) {
        fprintf(stderr,
            "Error: changing port state for multiple hubs at once is not supported.\n"
            "Use -l to limit operation to one hub!\n"
        );
        exit(1);
    }
    int k; /* k=0 for power OFF, k=1 for power ON */
    for (k=0; k<2; k++) { /* up to 2 power actions - off/on */
        if (k == 0 && opt_action == POWER_ON )
            continue;
        if (k == 1 && opt_action == POWER_OFF)
            continue;
        if (k == 1 && opt_action == POWER_KEEP)
            continue;
        // if toggle requested, do it only once when `k == 0`
        if (k == 1 && opt_action == POWER_TOGGLE)
            continue;
        int i;
        for (i=0; i<hub_count; i++) {
            if (hubs[i].actionable == 0)
                continue;
            printf("Current status for hub %s [%s]\n",
                hubs[i].location, hubs[i].ds.description
            );
            print_port_status(&hubs[i], opt_ports);
            if (opt_action == POWER_KEEP) { /* no action, show status */
                continue;
            }
            struct libusb_device_handle * devh = NULL;
            rc = libusb_open(hubs[i].dev, &devh);
            if (rc == 0) {
                /* will operate on these ports */
                int ports = ((1 << hubs[i].nports) - 1) & opt_ports;
                int request = (k == 0) ? LIBUSB_REQUEST_CLEAR_FEATURE
                                       : LIBUSB_REQUEST_SET_FEATURE;
                int port;
                for (port=1; port <= hubs[i].nports; port++) {
                    if ((1 << (port-1)) & ports) {
                        int port_status = get_port_status(devh, port);
                        int power_mask = hubs[i].super_speed ? USB_SS_PORT_STAT_POWER
                                                             : USB_PORT_STAT_POWER;
                        int powered_on = port_status & power_mask;
                        if (opt_action == POWER_TOGGLE) {
                            request = powered_on ? LIBUSB_REQUEST_CLEAR_FEATURE
                                                 : LIBUSB_REQUEST_SET_FEATURE;
                        }
                        if (k == 0 && !powered_on && opt_action != POWER_TOGGLE)
                            continue;
                        if (k == 1 && powered_on)
                            continue;
                        int repeat = powered_on ? opt_repeat : 1;
                        while (repeat-- > 0) {
                            rc = libusb_control_transfer(devh,
                                LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_OTHER,
                                request, USB_PORT_FEAT_POWER,
                                port, NULL, 0, USB_CTRL_GET_TIMEOUT
                            );
                            if (rc < 0) {
                                perror("Failed to control port power!\n");
                            }
                            if (repeat > 0) {
                                sleep_ms(opt_wait);
                            }
                        }
                    }
                }
                /* USB3 hubs need extra delay to actually turn off: */
                if (k==0 && hubs[i].super_speed)
                    sleep_ms(150);
                printf("Sent power %s request\n",
                    request == LIBUSB_REQUEST_CLEAR_FEATURE ? "off" : "on"
                );
                printf("New status for hub %s [%s]\n",
                    hubs[i].location, hubs[i].ds.description
                );
                print_port_status(&hubs[i], opt_ports);

                if (k == 1 && opt_reset == 1) {
                    printf("Resetting hub...\n");
                    rc = libusb_reset_device(devh);
                    if (rc < 0) {
                        perror("Reset failed!\n");
                    } else {
                        printf("Reset successful!\n");
                    }
                }
            }
            libusb_close(devh);
        }
        if (k == 0 && opt_action == POWER_CYCLE)
            sleep_ms((int)(opt_delay * 1000));
    }
    rc = 0;
cleanup:
    if (usb_devs)
        libusb_free_device_list(usb_devs, 1);
    usb_devs = NULL;
    libusb_exit(NULL);
    return rc;
}
