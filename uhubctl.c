/*
 * Copyright (c) 2009-2025 Vadim Mikhailov
 *
 * Utility to turn USB port power on/off
 * for USB hubs that support per-port power switching.
 *
 * This file can be distributed under the terms and conditions of the
 * GNU General Public License version 2.
 *
 */

#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

#include "mkjson.h"

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <process.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#endif

#include <libusb.h>

/* LIBUSBX_API_VERSION was first defined in libusb 1.0.13
  and renamed to LIBUSB_API_VERSION since libusb 1.0.16 */
#if defined(LIBUSBX_API_VERSION) && !defined(LIBUSB_API_VERSION)
#define LIBUSB_API_VERSION LIBUSBX_API_VERSION
#endif

/* FreeBSD's libusb does not define LIBUSB_DT_SUPERSPEED_HUB */
#if !defined(LIBUSB_DT_SUPERSPEED_HUB)
#define LIBUSB_DT_SUPERSPEED_HUB 0x2a
#endif

#if !defined(LIBUSB_API_VERSION)
#error "libusb-1.0 is required!"
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
#define POWER_FLASH              4

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


/*
 * USB Speed definitions
 * Reference: USB 3.2 Specification, Table 10-10
 */
#define USB_SPEED_UNKNOWN        0
#define USB_SPEED_LOW            1   /* USB 1.0/1.1 Low Speed: 1.5 Mbit/s */
#define USB_SPEED_FULL           2   /* USB 1.0/1.1 Full Speed: 12 Mbit/s */
#define USB_SPEED_HIGH           3   /* USB 2.0 High Speed: 480 Mbit/s */
#define USB_SPEED_SUPER          4   /* USB 3.0 SuperSpeed: 5 Gbit/s */
#define USB_SPEED_SUPER_PLUS     5   /* USB 3.1 SuperSpeed+: 10 Gbit/s */
#define USB_SPEED_SUPER_PLUS_20  6   /* USB 3.2 SuperSpeed+ 20Gbps: 20 Gbit/s */
#define USB_SPEED_USB4_20        7   /* USB4 20Gbps */
#define USB_SPEED_USB4_40        8   /* USB4 40Gbps */
#define USB_SPEED_USB4_80        9   /* USB4 Version 2.0: 80Gbps */

/*
 * USB Port StatusS Speed masks
 */
#define USB_PORT_STAT_SPEED_MASK     0x1C00

/*
 * USB 3.0 and 3.1 speed encodings
*/
#define USB_PORT_STAT_SPEED_5GBPS    0x0000
#define USB_PORT_STAT_SPEED_10GBPS   0x0400
#define USB_PORT_STAT_SPEED_20GBPS   0x0800

/*
 * Additional speed encodings for USB4
 */
#define USB_PORT_STAT_SPEED_40GBPS   0x0C00
#define USB_PORT_STAT_SPEED_80GBPS   0x1000

/* List of all USB devices enumerated by libusb */
static struct libusb_device **usb_devs = NULL;

struct descriptor_strings {
    char vendor[64];
    char product[64];
    char serial[64];
    char description[512];
    /* Additional fields for JSON output */
    uint16_t vid;
    uint16_t pid;
    uint8_t device_class;
    char class_name[64];
    uint16_t usb_version;
    uint16_t device_version;
    int is_mass_storage;
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
static char opt_searchhub[64] = "";    /* Search by hub description */
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
static int opt_json = 0;    /* output in JSON format */

#if defined(__linux__)
static int opt_nosysfs = 0; /* don't use the Linux sysfs port disable interface, even if available */
#if (LIBUSB_API_VERSION >= 0x01000107) /* 1.0.23 */
static const char *opt_sysdev;
#endif
#endif

/* For Raspberry Pi detection and workarounds: */
static int is_rpi_4b = 0;
static int is_rpi_5  = 0;

static const char short_options[] =
    "l:L:n:a:p:d:r:w:s:H:vefRNjh"
#if defined(__linux__)
    "S"
#if (LIBUSB_API_VERSION >= 0x01000107) /* 1.0.23 */
    "y:"
#endif
#endif
;

static const struct option long_options[] = {
    { "location", required_argument, NULL, 'l' },
    { "vendor",   required_argument, NULL, 'n' },
    { "search",   required_argument, NULL, 's' },
    { "searchhub",required_argument, NULL, 'H' },
    { "level",    required_argument, NULL, 'L' },
    { "ports",    required_argument, NULL, 'p' },
    { "action",   required_argument, NULL, 'a' },
    { "delay",    required_argument, NULL, 'd' },
    { "repeat",   required_argument, NULL, 'r' },
    { "wait",     required_argument, NULL, 'w' },
    { "exact",    no_argument,       NULL, 'e' },
    { "force",    no_argument,       NULL, 'f' },
    { "nodesc",   no_argument,       NULL, 'N' },
#if defined(__linux__)
    { "nosysfs",  no_argument,       NULL, 'S' },
#if (LIBUSB_API_VERSION >= 0x01000107)
    { "sysdev",   required_argument, NULL, 'y' },
#endif
#endif
    { "reset",    no_argument,       NULL, 'R' },
    { "version",  no_argument,       NULL, 'v' },
    { "json",     no_argument,       NULL, 'j' },
    { "help",     no_argument,       NULL, 'h' },

    { 0,          0,                 NULL, 0   },
};

/* Forward declarations */
static int is_mass_storage_device(struct libusb_device *dev);
static const char* get_primary_device_class_name(struct libusb_device *dev, struct libusb_device_descriptor *desc);
static struct libusb_device* find_device_on_hub_port(struct hub_info *hub, int port);

static int print_usage(void)
{
    printf(
        "uhubctl: utility to control USB port power for smart hubs.\n"
        "Usage: uhubctl [options]\n"
        "Without options, show status for all smart hubs.\n"
        "\n"
        "Options [defaults in brackets]:\n"
        "--action,   -a - action to off/on/cycle/toggle/flash (0/1/2/3/4) for affected ports.\n"
        "--ports,    -p - ports to operate on    [all hub ports].\n"
        "--location, -l - limit hub by location  [all smart hubs].\n"
        "--level     -L - limit hub by location level (e.g. a-b.c is level 3).\n"
        "--vendor,   -n - limit hub by vendor id [%s] (partial ok).\n"
        "--searchhub,-H - limit hub by description.\n"
        "--search,   -s - limit hub by attached device description.\n"
        "--delay,    -d - delay for cycle/flash action [%g sec].\n"
        "--repeat,   -r - repeat power off count [%d] (some devices need it to turn off).\n"
        "--exact,    -e - exact location (no USB3 duality handling).\n"
        "--force,    -f - force operation even on unsupported hubs.\n"
        "--nodesc,   -N - do not query device description (helpful for unresponsive devices).\n"
#if defined(__linux__)
        "--nosysfs,  -S - do not use the Linux sysfs port disable interface.\n"
#if (LIBUSB_API_VERSION >= 0x01000107)
        "--sysdev,   -y - open system device node instead of scanning.\n"
#endif
#endif
        "--reset,    -R - reset hub after each power-on action, causing all devices to reassociate.\n"
        "--wait,     -w - wait before repeat power off [%d ms].\n"
        "--json,     -j - output in JSON format.\n"
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

static char* trim(char* str)
{
    /* Trim leading spaces by moving content to the beginning */
    char* start = str;
    while (isspace(*start)) {
        start++;
    }

    /* Move content to beginning if there were leading spaces */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    /* Trim trailing spaces */
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
 * Get model of the computer we are currently running on.
 * On success return 0 and fill model string (null terminated).
 * If model is not known or error occurred returns -1.
 *
 * Currently this can only return successfully on Linux,
 * but in the future we may need it on other operating systems too.
 */

static int get_computer_model(char *model, int len)
{
    int fd = open("/sys/firmware/devicetree/base/model", O_RDONLY);
    if (fd >= 0) {
        int bytes_read = read(fd, model, len-1);
        close(fd);
        if (bytes_read < 0) {
            return -1;
        }
        model[bytes_read] = 0;
    } else {
        /* devicetree is not available, try parsing /proc/cpuinfo instead. */
        /* most Raspberry Pi have /proc/cpuinfo about 1KB, so 4KB buffer should be plenty: */
        char buffer[4096] = {0}; /* fill buffer with all zeros */
        fd = open("/proc/cpuinfo", O_RDONLY);
        if (fd < 0) {
            return -1;
        }
        int bytes_read = read(fd, buffer, sizeof(buffer)-1);
        close(fd);
        if (bytes_read < 0) {
            return -1;
        }
        buffer[bytes_read] = 0;
        char* model_start = strstr(buffer, "Model\t\t: ");
        if (model_start == NULL) {
            return -1;
        }
        char* model_name = model_start + 9;
        char* newline_pos = strchr(model_name, '\n');
        if (newline_pos != NULL) {
            *newline_pos = 0;
        }
        strncpy(model, model_name, len);
        model[len-1] = 0;
    }
    return 0;
}

/*
 * Check if we are running on given computer model using substring match.
 * Returns 1 if yes and 0 otherwise.
 */

static int check_computer_model(const char *target)
{
    char model[256] = "";
    if (get_computer_model(model, sizeof(model)) == 0) {
        if (strstr(model, target) != NULL) {
            return 1;
        }
    }
    return 0;
}


/*
 * Compatibility wrapper around libusb_get_port_numbers()
 */

static int get_port_numbers(libusb_device *dev, uint8_t *buf, uint8_t bufsize)
{
    int pcount;
#if (LIBUSB_API_VERSION >= 0x01000102)
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
    int bcd_usb = desc.bcdUSB;
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
                desc.idVendor,
                desc.idProduct
            );

            /* Convert bus and ports array into USB location string */
            info->bus = libusb_get_bus_number(dev);
            snprintf(info->location, sizeof(info->location), "%d", info->bus);
            info->pn_len = get_port_numbers(dev, info->port_numbers, sizeof(info->port_numbers));
            int k;
            for (k=0; k < info->pn_len; k++) {
                char s[8];
                snprintf(s, sizeof(s), "%s%d", k==0 ? "-" : ".", info->port_numbers[k]);
                strncat(info->location, s, sizeof(info->location) - strlen(info->location) - 1);
            }

            /* Get container_id: */
            memset(info->container_id, 0, sizeof(info->container_id));
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
                if (is_rpi_4b &&
                    strlen(info->container_id)==0 &&
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
            if (is_rpi_4b && lpsm == HUB_CHAR_COMMON_LPSM && strcasecmp(info->vendor, "2109:3431")==0) {
                lpsm = HUB_CHAR_INDV_PORT_LPSM;
            }
            info->lpsm = lpsm;

            /* Raspberry Pi 5 hack */
            if (is_rpi_5 &&
                strlen(info->container_id)==0 &&
                info->lpsm==HUB_CHAR_INDV_PORT_LPSM &&
                info->pn_len==0)
            {
                /* USB2 */
                if (strcasecmp(info->vendor, "1d6b:0002")==0 &&
                    info->nports==2 &&
                    !info->super_speed)
                {
                    strcpy(info->container_id, "Raspberry Pi 5 Fake Container Id");
                }
                /* USB3 */
                if (strcasecmp(info->vendor, "1d6b:0003")==0 &&
                    info->nports==1 &&
                    info->super_speed)
                {
                    strcpy(info->container_id, "Raspberry Pi 5 Fake Container Id");
                }
            }
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
    return libusb_le16_to_cpu(ust.wPortStatus);
}


#if defined(__linux__)
/*
 * Try to use the Linux sysfs interface to power a port off/on.
 * Returns 0 on success.
 */

static int set_port_status_linux(struct libusb_device_handle *devh, struct hub_info *hub, int port, int on)
{
    int configuration = 0;
    char disable_path[PATH_MAX];

    int rc = libusb_get_configuration(devh, &configuration);
    if (rc < 0) {
        return rc;
    }

    /*
     * The "disable" sysfs interface is available only starting with kernel version 6.0.
     * For earlier kernel versions the open() call will fail and we fall back to using libusb.
     */
    if (hub->pn_len == 0) {
      snprintf(disable_path, PATH_MAX,
          "/sys/bus/usb/devices/%s-0:%d.0/usb%s-port%i/disable",
          hub->location, configuration, hub->location, port
      );
    } else {
      snprintf(disable_path, PATH_MAX,
          "/sys/bus/usb/devices/%s:%d.0/%s-port%i/disable",
          hub->location, configuration, hub->location, port
      );
    }

    int disable_fd = open(disable_path, O_WRONLY);
    if (disable_fd >= 0) {
        rc = write(disable_fd, on ? "0" : "1", 1);
        close(disable_fd);
    }

    if (disable_fd < 0 || rc < 0) {
        /*
         * ENOENT is the expected error when running on Linux kernel < 6.0 where
         * sysfs disable interface does not exist yet - no need to report anything in this case.
         * If the file exists but another error occurs it is most likely a permission issue.
         * Print an error message mostly geared towards setting up udev.
         */
        if (errno != ENOENT) {
            fprintf(stderr,
                "Failed to set port status by writing to %s (%s).\n"
                "Follow https://git.io/JIB2Z to make sure that udev is set up correctly.\n"
                "Falling back to libusb based port control.\n"
                "Use -S to skip trying the sysfs interface and printing this message.\n",
                disable_path, strerror(errno)
            );
        }

        return -1;
    }

    return 0;
}
#endif


/*
 * Use a control transfer via libusb to turn a port off/on.
 * Returns >= 0 on success.
 */

static int set_port_status_libusb(struct libusb_device_handle *devh, int port, int on)
{
    int rc = 0;
    int request = on ? LIBUSB_REQUEST_SET_FEATURE
                     : LIBUSB_REQUEST_CLEAR_FEATURE;
    int repeat = on ? 1 : opt_repeat;

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

    return rc;
}


/*
 * Try different methods to power a port off/on.
 * Return >= 0 on success.
 */

static int set_port_status(struct libusb_device_handle *devh, struct hub_info *hub, int port, int on)
{
#if defined(__linux__)
    if (!opt_nosysfs) {
        if (set_port_status_linux(devh, hub, port, on) == 0) {
            return 0;
        }
    }
#else
    (void)hub;
#endif

    return set_port_status_libusb(devh, port, on);
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
    memset(ds, 0, sizeof(*ds));
    id_vendor  = desc.idVendor;
    id_product = desc.idProduct;
    rc = libusb_open(dev, &devh);
    if (rc == 0) {
        if (!opt_nodesc) {
            if (desc.iManufacturer) {
                rc = libusb_get_string_descriptor_ascii(devh,
                    desc.iManufacturer, (unsigned char*)ds->vendor, sizeof(ds->vendor));
                trim(ds->vendor);
            }
            if (rc >= 0 && desc.iProduct) {
                rc = libusb_get_string_descriptor_ascii(devh,
                    desc.iProduct, (unsigned char*)ds->product, sizeof(ds->product));
                trim(ds->product);
            }
            if (rc >= 0 && desc.iSerialNumber) {
                rc = libusb_get_string_descriptor_ascii(devh,
                    desc.iSerialNumber, (unsigned char*)ds->serial, sizeof(ds->serial));
                rtrim(ds->serial);
            }
        }
        if (desc.bDeviceClass == LIBUSB_CLASS_HUB) {
            struct hub_info info;
            memset(&info, 0, sizeof(info));
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
    
    /* Populate additional fields for JSON output */
    ds->vid = desc.idVendor;
    ds->pid = desc.idProduct;
    ds->device_class = desc.bDeviceClass;
    ds->usb_version = desc.bcdUSB;
    ds->device_version = desc.bcdDevice;
    ds->is_mass_storage = (dev && is_mass_storage_device(dev)) ? 1 : 0;
    
    /* Get device class name */
    const char* class_name = get_primary_device_class_name(dev, &desc);
    strncpy(ds->class_name, class_name, sizeof(ds->class_name) - 1);
    ds->class_name[sizeof(ds->class_name) - 1] = '\0';
    
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

/* Helper function to find a device connected to a specific hub port */
static struct libusb_device* find_device_on_hub_port(struct hub_info *hub, int port)
{
    struct libusb_device *udev = NULL;
    int i = 0;
    
    while ((udev = usb_devs[i++]) != NULL) {
        uint8_t dev_bus = libusb_get_bus_number(udev);
        if (dev_bus != hub->bus) continue;
        
        uint8_t dev_pn[MAX_HUB_CHAIN];
        int dev_plen = get_port_numbers(udev, dev_pn, sizeof(dev_pn));
        if ((dev_plen == hub->pn_len + 1) &&
            (memcmp(hub->port_numbers, dev_pn, hub->pn_len) == 0) &&
            libusb_get_port_number(udev) == port)
        {
            return udev;
        }
    }
    return NULL;
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
            memset(&ds, 0, sizeof(ds));
            struct libusb_device *udev = find_device_on_hub_port(hub, port);
            if (udev) {
                get_device_description(udev, &ds);
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
                int power_mask = hub->super_speed ? USB_SS_PORT_STAT_POWER : USB_PORT_STAT_POWER;
                if (!(port_status & power_mask)) {
                    printf(" off");
                } else {
                    int link_state = port_status & USB_PORT_STAT_LINK_STATE;
                    if (port_status & power_mask)                 printf(" power");
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

static int usb_find_hubs(void)
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
        memset(&info, 0, sizeof(info));
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
                    memset(&ds, 0, sizeof(ds));
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
        if (strlen(opt_searchhub) > 0) {
            /* Search by hub description */
            if (strstr(info.ds.description, opt_searchhub) == NULL) {
                info.actionable = 0;
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
                if (is_rpi_4b && l1 + s1 == l2 + s2 && l1 >= s2 && memcmp(p1 + s2, p2 + s1, l1 - s2)==0) {
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
#if defined(__linux__)
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

int is_mass_storage_device(struct libusb_device *dev)
{
    struct libusb_config_descriptor *config;
    int rc = 0;
    if (libusb_get_config_descriptor(dev, 0, &config) == 0) {
        for (int i = 0; i < config->bNumInterfaces; i++) {
            const struct libusb_interface *interface = &config->interface[i];
            for (int j = 0; j < interface->num_altsetting; j++) {
                const struct libusb_interface_descriptor *altsetting = &interface->altsetting[j];
                if (altsetting->bInterfaceClass == LIBUSB_CLASS_MASS_STORAGE) {
                    rc = 1;
                    goto out;
                }
            }
        }
    out:
        libusb_free_config_descriptor(config);
    }
    return rc;
}




/* Helper function to determine port speed */
void get_port_speed(int port_status, char** speed_str, int64_t* speed_bps, int super_speed)
{
    *speed_str = "Disconnected";
    *speed_bps = 0;

    if (port_status & USB_PORT_STAT_CONNECTION) {
        /* Check if this is a USB3 hub first */
        if (super_speed) {
            int speed_mask = port_status & USB_PORT_STAT_SPEED_MASK;
            switch (speed_mask) {
                case USB_PORT_STAT_SPEED_5GBPS:
                    *speed_str = "USB3.0 SuperSpeed 5 Gbps";
                    *speed_bps = 5000000000LL;
                    break;
                case USB_PORT_STAT_SPEED_10GBPS:
                    *speed_str = "USB 3.1 Gen 2 SuperSpeed+ 10 Gbps";
                    *speed_bps = 10000000000LL;
                    break;
                case USB_PORT_STAT_SPEED_20GBPS:
                    *speed_str = "USB 3.2 Gen 2x2 SuperSpeed+ 20 Gbps";
                    *speed_bps = 20000000000LL;
                    break;
                case USB_PORT_STAT_SPEED_40GBPS:
                    *speed_str = "USB4 40 Gbps";
                    *speed_bps = 40000000000LL;
                    break;
                case USB_PORT_STAT_SPEED_80GBPS:
                    *speed_str = "USB4 80 Gbps";
                    *speed_bps = 80000000000LL;
                    break;
                default:
                    *speed_str = "USB1.1 Full Speed 12Mbps";
                    *speed_bps = 12000000; /* 12 Mbit/s (default for USB 1.1) */
            }
        } else {
            /* USB2 port - check speed bits */
            if (port_status & USB_PORT_STAT_LOW_SPEED) {
                *speed_str = "USB1.0 Low Speed 1.5 Mbps";
                *speed_bps = 1500000; /* 1.5 Mbit/s */
            } else if (port_status & USB_PORT_STAT_HIGH_SPEED) {
                *speed_str = "USB2.0 High Speed 480Mbps";
                *speed_bps = 480000000; /* 480 Mbit/s */
            } else {
                /* USB 2.0 Full Speed (neither low nor high speed) */
                *speed_str = "USB1.1 Full Speed 12Mbps";
                *speed_bps = 12000000; /* 12 Mbit/s */
            }
        }
    }
}

/* Helper function to get class name */
const char* get_class_name(uint8_t class_code)
{
    switch(class_code) {
        case LIBUSB_CLASS_PER_INTERFACE:
            return "Per Interface";
        case LIBUSB_CLASS_AUDIO:
            return "Audio";
        case LIBUSB_CLASS_COMM:
            return "Communications";
        case LIBUSB_CLASS_HID:
            return "Human Interface Device";
        case LIBUSB_CLASS_PHYSICAL:
            return "Physical";
        case LIBUSB_CLASS_PRINTER:
            return "Printer";
        case LIBUSB_CLASS_IMAGE:
            return "Image";
        case LIBUSB_CLASS_MASS_STORAGE:
            return "Mass Storage";
        case LIBUSB_CLASS_HUB:
            return "Hub";
        case LIBUSB_CLASS_DATA:
            return "Data";
        case LIBUSB_CLASS_SMART_CARD:
            return "Smart Card";
        case LIBUSB_CLASS_CONTENT_SECURITY:
            return "Content Security";
        case LIBUSB_CLASS_VIDEO:
            return "Video";
        case LIBUSB_CLASS_PERSONAL_HEALTHCARE:
            return "Personal Healthcare";
        case LIBUSB_CLASS_DIAGNOSTIC_DEVICE:
            return "Diagnostic Device";
        case LIBUSB_CLASS_WIRELESS:
            return "Wireless";
        case LIBUSB_CLASS_APPLICATION:
            return "Application";
        case LIBUSB_CLASS_VENDOR_SPEC:
            return "Vendor Specific";
        default:
            return "Unknown";
    }
}
const char* get_primary_device_class_name(struct libusb_device *dev, struct libusb_device_descriptor *desc)
{
    if (desc->bDeviceClass != LIBUSB_CLASS_PER_INTERFACE) {
        return get_class_name(desc->bDeviceClass);
    }

    struct libusb_config_descriptor *config;
    if (libusb_get_config_descriptor(dev, 0, &config) != 0) {
        return "Unknown";
    }

    const char* primary_class = "Composite Device";
    for (int i = 0; i < config->bNumInterfaces; i++) {
        const struct libusb_interface *interface = &config->interface[i];
        for (int j = 0; j < interface->num_altsetting; j++) {
            const struct libusb_interface_descriptor *altsetting = &interface->altsetting[j];
            const char* interface_class_name = get_class_name(altsetting->bInterfaceClass);

            /* Prioritized classes */
            switch (altsetting->bInterfaceClass) {
                case LIBUSB_CLASS_HID:
                    libusb_free_config_descriptor(config);
                    return interface_class_name; /* Human Interface Device */
                case LIBUSB_CLASS_MASS_STORAGE:
                    primary_class = interface_class_name; /* Mass Storage */
                    break;
                case LIBUSB_CLASS_AUDIO:
                case LIBUSB_CLASS_VIDEO:
                    libusb_free_config_descriptor(config);
                    return interface_class_name; /* Audio or Video */
                case LIBUSB_CLASS_PRINTER:
                    libusb_free_config_descriptor(config);
                    return interface_class_name; /* Printer */
                case LIBUSB_CLASS_COMM:
                case LIBUSB_CLASS_DATA:
                    if (strcmp(primary_class, "Composite Device") == 0) {
                        primary_class = "Communications"; /* CDC devices often have both COMM and DATA interfaces */
                    }
                    break;
                case LIBUSB_CLASS_SMART_CARD:
                    libusb_free_config_descriptor(config);
                    return interface_class_name; /* Smart Card */
                case LIBUSB_CLASS_CONTENT_SECURITY:
                    libusb_free_config_descriptor(config);
                    return interface_class_name; /* Content Security */
                case LIBUSB_CLASS_WIRELESS:
                    if (strcmp(primary_class, "Composite Device") == 0) {
                        primary_class = interface_class_name; /* Wireless Controller */
                    }
                    break;
                case LIBUSB_CLASS_APPLICATION:
                    if (strcmp(primary_class, "Composite Device") == 0) {
                        primary_class = interface_class_name; /* Application Specific */
                    }
                    break;
                /* Add more cases here if needed */
            }
        }
    }

    libusb_free_config_descriptor(config);
    return primary_class;
}

/* Helper function to create the status flags JSON object (using mkjson) */
/* Only outputs flags that are true to reduce JSON size */
/* Returns: Allocated JSON string. Caller must free() the returned string. */
char* create_status_flags_json(int port_status, int super_speed)
{
    struct {
        int mask;
        const char* name;
    } flag_defs[] = {
        {USB_PORT_STAT_CONNECTION, "connection"},
        {USB_PORT_STAT_ENABLE, "enable"},
        {USB_PORT_STAT_SUSPEND, "suspend"},
        {USB_PORT_STAT_OVERCURRENT, "overcurrent"},
        {USB_PORT_STAT_RESET, "reset"},
        {super_speed ? USB_SS_PORT_STAT_POWER : USB_PORT_STAT_POWER, "power"},
        {super_speed ? 0 : USB_PORT_STAT_LOW_SPEED, "lowspeed"},
        {super_speed ? 0 : USB_PORT_STAT_HIGH_SPEED, "highspeed"},
        {USB_PORT_STAT_TEST, "test"},
        {USB_PORT_STAT_INDICATOR, "indicator"},
        {0, NULL}
    };

    /* Calculate exact buffer size needed for flags JSON */
    size_t buffer_size = 3; /* "{}" + null terminator */
    int active_count = 0;

    for (int i = 0; flag_defs[i].name != NULL; i++) {
        if ((flag_defs[i].mask != 0) && (port_status & flag_defs[i].mask)) {
            if (active_count > 0) buffer_size += 2; /* ", " */
            buffer_size += 1 + strlen(flag_defs[i].name) + 7; /* "name": true */
            active_count++;
        }
    }

    char* result = malloc(buffer_size);
    if (!result) return NULL;

    char* ptr = result;
    int remaining = buffer_size;

    int written = snprintf(ptr, remaining, "{");
    ptr += written;
    remaining -= written;

    int first = 1;
    for (int i = 0; flag_defs[i].name != NULL; i++) {
        if ((flag_defs[i].mask != 0) && (port_status & flag_defs[i].mask)) {
            written = snprintf(ptr, remaining, "%s\"%s\": true",
                               first ? "" : ", ", flag_defs[i].name);
            ptr += written;
            remaining -= written;
            first = 0;
        }
    }

    snprintf(ptr, remaining, "}");
    return result;
}

/* Helper function to create human-readable descriptions of set flags */
/* Returns: Allocated JSON string. Caller must free() the returned string. */
char* create_human_readable_json(int port_status, int super_speed)
{
    struct {
        int mask;
        const char* name;
        const char* description;
    } flag_defs[] = {
        {USB_PORT_STAT_CONNECTION, "connection", "Device is connected"},
        {USB_PORT_STAT_ENABLE, "enable", "Port is enabled"},
        {USB_PORT_STAT_SUSPEND, "suspend", "Port is suspended"},
        {USB_PORT_STAT_OVERCURRENT, "overcurrent", "Over-current condition exists"},
        {USB_PORT_STAT_RESET, "reset", "Port is in reset state"},
        {super_speed ? USB_SS_PORT_STAT_POWER : USB_PORT_STAT_POWER, "power", "Port power is enabled"},
        {super_speed ? 0 : USB_PORT_STAT_LOW_SPEED, "lowspeed", "Low-speed device attached"},
        {super_speed ? 0 : USB_PORT_STAT_HIGH_SPEED, "highspeed", "High-speed device attached"},
        {USB_PORT_STAT_TEST, "test", "Port is in test mode"},
        {USB_PORT_STAT_INDICATOR, "indicator", "Port indicator control"},
        {0, NULL, NULL}
    };

    /* Calculate exact buffer size needed for human_readable JSON */
    size_t buffer_size = 3; /* "{}" + null terminator */
    int active_count = 0;

    for (int i = 0; flag_defs[i].name != NULL; i++) {
        if ((flag_defs[i].mask != 0) && (port_status & flag_defs[i].mask)) {
            if (active_count > 0) buffer_size += 2; /* ", " */
            buffer_size += 1 + strlen(flag_defs[i].name) + 4; /* "name": " */
            buffer_size += strlen(flag_defs[i].description) + 1; /* description" */
            active_count++;
        }
    }

    char* result = malloc(buffer_size);
    if (!result) return NULL;

    char* ptr = result;
    int remaining = buffer_size;

    int written = snprintf(ptr, remaining, "{");
    ptr += written;
    remaining -= written;

    int first = 1;
    for (int i = 0; flag_defs[i].name != NULL; i++) {
        if ((flag_defs[i].mask != 0) && (port_status & flag_defs[i].mask)) {
            written = snprintf(ptr, remaining, "%s\"%s\": \"%s\"",
                               first ? "" : ", ", flag_defs[i].name, flag_defs[i].description);
            ptr += written;
            remaining -= written;
            first = 0;
        }
    }

    snprintf(ptr, remaining, "}");
    return result;
}



/* Helper function to create status bits JSON object */
/* Returns: Allocated JSON string. Caller must free() the returned string. */
char* create_status_bits_json(int port_status, int super_speed)
{
    int power_mask = super_speed ? USB_SS_PORT_STAT_POWER : USB_PORT_STAT_POWER;

    mkjson_arg bits_args[] = {
        { MKJSON_BOOL, "connection", .value.bool_val = (port_status & USB_PORT_STAT_CONNECTION) != 0 },
        { MKJSON_BOOL, "enabled", .value.bool_val = (port_status & USB_PORT_STAT_ENABLE) != 0 },
        { MKJSON_BOOL, "powered", .value.bool_val = (port_status & power_mask) != 0 },
        { MKJSON_BOOL, "suspended", .value.bool_val = (port_status & USB_PORT_STAT_SUSPEND) != 0 },
        { MKJSON_BOOL, "overcurrent", .value.bool_val = (port_status & USB_PORT_STAT_OVERCURRENT) != 0 },
        { MKJSON_BOOL, "reset", .value.bool_val = (port_status & USB_PORT_STAT_RESET) != 0 },
        { MKJSON_BOOL, "highspeed", .value.bool_val = !super_speed && (port_status & USB_PORT_STAT_HIGH_SPEED) != 0 },
        { MKJSON_BOOL, "lowspeed", .value.bool_val = !super_speed && (port_status & USB_PORT_STAT_LOW_SPEED) != 0 },
        { 0 }
    };

    return mkjson_array_pretty(MKJSON_OBJ, bits_args, 4);
}

/* Helper function to decode port status into human-readable string */
const char* decode_port_status(int port_status, int super_speed)
{
    if (port_status == 0x0000) return "no_power";

    int power_mask = super_speed ? USB_SS_PORT_STAT_POWER : USB_PORT_STAT_POWER;
    int has_power = (port_status & power_mask) != 0;
    int has_connection = (port_status & USB_PORT_STAT_CONNECTION) != 0;
    int is_enabled = (port_status & USB_PORT_STAT_ENABLE) != 0;
    int is_suspended = (port_status & USB_PORT_STAT_SUSPEND) != 0;
    int has_overcurrent = (port_status & USB_PORT_STAT_OVERCURRENT) != 0;
    int in_reset = (port_status & USB_PORT_STAT_RESET) != 0;

    if (has_overcurrent) return "overcurrent";
    if (in_reset) return "resetting";
    if (!has_power) return "no_power";
    if (!has_connection) return "powered_no_device";
    if (!is_enabled) return "device_connected_not_enabled";
    if (is_suspended) return "device_suspended";
    return "device_active";
}

/* Create complete port status JSON string using mkjson */
/* Returns: Allocated JSON string. Caller must free() the returned string. */
char* create_port_status_json(int port, int port_status, const struct descriptor_strings* ds, struct libusb_device *dev, int super_speed)
{
    char status_hex[7];
    snprintf(status_hex, sizeof(status_hex), "0x%04x", port_status);

    char* speed_str;
    int64_t speed_bps;
    get_port_speed(port_status, &speed_str, &speed_bps, super_speed);

    /* Create status object */
    const char* status_decoded = decode_port_status(port_status, super_speed);
    char* status_bits_json = create_status_bits_json(port_status, super_speed);

    mkjson_arg status_args[] = {
        { MKJSON_STRING, "raw", .value.str_val = status_hex },
        { MKJSON_STRING, "decoded", .value.str_val = status_decoded },
        { MKJSON_JSON_FREE, "bits", .value.str_free_val = status_bits_json },
        { 0 }
    };
    char* status_json = mkjson_array_pretty(MKJSON_OBJ, status_args, 4);

    /* Get sub-objects */
    char *flags_json = create_status_flags_json(port_status, super_speed);
    char *hr_json = create_human_readable_json(port_status, super_speed);

    /* For USB3 hubs, get link state and port speed */
    const char* link_state_str = NULL;
    const char* port_speed = NULL;
    if (super_speed) {
        /* Check if this is a 5Gbps capable port */
        if ((port_status & USB_SS_PORT_STAT_SPEED) == USB_PORT_STAT_SPEED_5GBPS) {
            port_speed = "5gbps";
        }

        int link_state = port_status & USB_PORT_STAT_LINK_STATE;
        switch (link_state) {
            case USB_SS_PORT_LS_U0:          link_state_str = "U0"; break;
            case USB_SS_PORT_LS_U1:          link_state_str = "U1"; break;
            case USB_SS_PORT_LS_U2:          link_state_str = "U2"; break;
            case USB_SS_PORT_LS_U3:          link_state_str = "U3"; break;
            case USB_SS_PORT_LS_SS_DISABLED: link_state_str = "SS.Disabled"; break;
            case USB_SS_PORT_LS_RX_DETECT:   link_state_str = "Rx.Detect"; break;
            case USB_SS_PORT_LS_SS_INACTIVE: link_state_str = "SS.Inactive"; break;
            case USB_SS_PORT_LS_POLLING:     link_state_str = "Polling"; break;
            case USB_SS_PORT_LS_RECOVERY:    link_state_str = "Recovery"; break;
            case USB_SS_PORT_LS_HOT_RESET:   link_state_str = "HotReset"; break;
            case USB_SS_PORT_LS_COMP_MOD:    link_state_str = "Compliance"; break;
            case USB_SS_PORT_LS_LOOPBACK:    link_state_str = "Loopback"; break;
        }
    }

    /* Basic port info without device */
    if (!(port_status & USB_PORT_STAT_CONNECTION) || !dev) {
        mkjson_arg basic_args[10]; /* Max possible args */
        int arg_idx = 0;

        basic_args[arg_idx++] = (mkjson_arg){ MKJSON_INT, "port", .value.int_val = port };
        basic_args[arg_idx++] = (mkjson_arg){ MKJSON_JSON_FREE, "status", .value.str_free_val = status_json };
        basic_args[arg_idx++] = (mkjson_arg){ MKJSON_JSON_FREE, "flags", .value.str_free_val = flags_json };
        basic_args[arg_idx++] = (mkjson_arg){ MKJSON_JSON_FREE, "human_readable", .value.str_free_val = hr_json };
        basic_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "speed", .value.str_val = speed_str };
        basic_args[arg_idx++] = (mkjson_arg){ MKJSON_LLINT, "speed_bps", .value.llint_val = speed_bps };

        if (port_speed) {
            basic_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "port_speed", .value.str_val = port_speed };
        }
        if (link_state_str) {
            basic_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "link_state", .value.str_val = link_state_str };
        }
        basic_args[arg_idx] = (mkjson_arg){ 0 }; /* Null terminator */

        char *result = mkjson_array_pretty(MKJSON_OBJ, basic_args, 2);

        return result;
    }

    /* Port with device - add device info */
    /* Note: device descriptor info is already available in ds from get_device_description */

    /* Use device info from descriptor_strings (already populated by get_device_description) */
    char vendor_id[8], product_id[8];
    snprintf(vendor_id, sizeof(vendor_id), "0x%04x", ds->vid);
    snprintf(product_id, sizeof(product_id), "0x%04x", ds->pid);

    /* Build USB and device versions */
    char usb_version[8], device_version[8];
    snprintf(usb_version, sizeof(usb_version), "%x.%02x", ds->usb_version >> 8, ds->usb_version & 0xFF);
    snprintf(device_version, sizeof(device_version), "%x.%02x", ds->device_version >> 8, ds->device_version & 0xFF);

    mkjson_arg device_args[25]; /* Max possible args */
    int arg_idx = 0;

    /* Basic port info */
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_INT, "port", .value.int_val = port };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_JSON_FREE, "status", .value.str_free_val = status_json };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_JSON_FREE, "flags", .value.str_free_val = flags_json };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_JSON_FREE, "human_readable", .value.str_free_val = hr_json };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "speed", .value.str_val = speed_str };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_LLINT, "speed_bps", .value.llint_val = speed_bps };

    /* Optional port info */
    if (port_speed) {
        device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "port_speed", .value.str_val = port_speed };
    }
    if (link_state_str) {
        device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "link_state", .value.str_val = link_state_str };
    }

    /* Device identifiers */
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "vid", .value.str_val = vendor_id };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "pid", .value.str_val = product_id };

    /* Optional vendor/product strings */
    if (ds->vendor[0]) {
        device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "vendor", .value.str_val = ds->vendor };
    }
    if (ds->product[0]) {
        device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "product", .value.str_val = ds->product };
    }

    /* Device class info */
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_INT, "device_class", .value.int_val = ds->device_class };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "class_name", .value.str_val = ds->class_name };

    /* Version info */
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "usb_version", .value.str_val = usb_version };
    device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "device_version", .value.str_val = device_version };

    /* Optional serial */
    if (ds->serial[0]) {
        device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "serial", .value.str_val = ds->serial };
    }

    /* Optional mass storage flag */
    if (ds->is_mass_storage) {
        device_args[arg_idx++] = (mkjson_arg){ MKJSON_BOOL, "is_mass_storage", .value.bool_val = ds->is_mass_storage };
    }

    device_args[arg_idx++] = (mkjson_arg){ MKJSON_STRING, "description", .value.str_val = ds->description[0] ? ds->description : NULL };

    device_args[arg_idx] = (mkjson_arg){ 0 }; /* Null terminator */

    char *result = mkjson_array_pretty(MKJSON_OBJ, device_args, 2);

    return result;
}

/* Create JSON representation of a hub and its ports */
/* Returns: Allocated JSON string. Caller must free() the returned string. */
char* create_hub_json(struct hub_info* hub, int portmask)
{
    unsigned int vendor_id, product_id;
    sscanf(hub->vendor, "%x:%x", &vendor_id, &product_id);

    char vendor_id_hex[8], product_id_hex[8];
    snprintf(vendor_id_hex, sizeof(vendor_id_hex), "0x%04x", vendor_id);
    snprintf(product_id_hex, sizeof(product_id_hex), "0x%04x", product_id);

    char usb_version[16];
    snprintf(usb_version, sizeof(usb_version), "%x.%02x", hub->bcd_usb >> 8, hub->bcd_usb & 0xFF);

    const char* power_switching_mode;
    switch (hub->lpsm) {
        case HUB_CHAR_INDV_PORT_LPSM:
            power_switching_mode = "ppps";
            break;
        case HUB_CHAR_COMMON_LPSM:
            power_switching_mode = "ganged";
            break;
        default:
            power_switching_mode = "nops";
    }

    /* Create hub_info object */
    mkjson_arg hub_info_args[] = {
        { MKJSON_STRING, "vid", .value.str_val = vendor_id_hex },
        { MKJSON_STRING, "pid", .value.str_val = product_id_hex },
        { MKJSON_STRING, "usb_version", .value.str_val = usb_version },
        { MKJSON_INT, "nports", .value.int_val = hub->nports },
        { MKJSON_STRING, "ppps", .value.str_val = power_switching_mode },
        { 0 }
    };
    char *hub_info_json = mkjson_array_pretty(MKJSON_OBJ, hub_info_args, 2);

    /* Create ports array */
    char *ports_array = NULL;
    char *port_jsons[MAX_HUB_PORTS];
    int valid_ports = 0;

    struct libusb_device_handle* devh = NULL;
    int rc = libusb_open(hub->dev, &devh);
    if (rc == 0) {
        for (int port = 1; port <= hub->nports; port++) {
            if (portmask > 0 && (portmask & (1 << (port-1))) == 0) continue;

            int port_status = get_port_status(devh, port);
            if (port_status == -1) continue;

            struct descriptor_strings ds;
            bzero(&ds, sizeof(ds));
            struct libusb_device* udev = find_device_on_hub_port(hub, port);
            if (udev) {
                get_device_description(udev, &ds);
            }

            port_jsons[valid_ports] = create_port_status_json(port, port_status, &ds, udev, hub->super_speed);
            valid_ports++;
        }
        libusb_close(devh);
    }

    /* Build the ports array manually */
    if (valid_ports == 0) {
        mkjson_arg empty_args[] = { { 0 } };
        ports_array = mkjson_array_pretty(MKJSON_ARR, empty_args, 2);
    } else {
        /* Calculate total size needed */
        int total_size = 3; /* "[]" + null terminator */
        for (int i = 0; i < valid_ports; i++) {
            total_size += strlen(port_jsons[i]);
            if (i > 0) total_size += 2; /* ", " */
        }

        ports_array = malloc(total_size);
        if (!ports_array) {
            for (int i = 0; i < valid_ports; i++) {
                free(port_jsons[i]);
            }
            return NULL;
        }

        char* ptr = ports_array;
        int remaining = total_size;
        int written = snprintf(ptr, remaining, "[");
        ptr += written;
        remaining -= written;
        for (int i = 0; i < valid_ports; i++) {
            written = snprintf(ptr, remaining, "%s%s", i > 0 ? ", " : "", port_jsons[i]);
            ptr += written;
            remaining -= written;
            free(port_jsons[i]);
        }
        snprintf(ptr, remaining, "]");
    }

    /* Create the final hub object */
    mkjson_arg hub_args[] = {
        { MKJSON_STRING, "location", .value.str_val = hub->location },
        { MKJSON_STRING, "description", .value.str_val = hub->ds.description },
        { MKJSON_JSON, "hub_info", .value.str_val = hub_info_json },
        { MKJSON_JSON, "ports", .value.str_val = ports_array },
        { 0 }
    };
    char *hub_json = mkjson_array_pretty(MKJSON_OBJ, hub_args, 2);

    free(hub_info_json);
    free(ports_array);

    return hub_json;
}




int main(int argc, char *argv[])
{
    int rc;
    int c = 0;
    int option_index = 0;
#if defined(__linux__) && (LIBUSB_API_VERSION >= 0x01000107)
    int sys_fd;
    libusb_device_handle *sys_devh = NULL;
#endif

    /* Initialize opt_action to POWER_KEEP */
    opt_action = POWER_KEEP;

    for (;;) {
        c = getopt_long(argc, argv, short_options, long_options, &option_index);
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
            snprintf(opt_location, sizeof(opt_location), "%s", optarg);
            break;
        case 'L':
            opt_level = atoi(optarg);
            break;
        case 'n':
            snprintf(opt_vendor, sizeof(opt_vendor), "%s", optarg);
            break;
        case 's':
            snprintf(opt_search, sizeof(opt_search), "%s", optarg);
            break;
        case 'H':
            snprintf(opt_searchhub, sizeof(opt_searchhub), "%s", optarg);
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
            if (!strcasecmp(optarg, "flash") || !strcasecmp(optarg, "4")) {
                opt_action = POWER_FLASH;
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
#if defined(__linux__)
        case 'S':
            opt_nosysfs = 1;
            break;
#if (LIBUSB_API_VERSION >= 0x01000107)
        case 'y':
            opt_sysdev = optarg;
            break;
#endif
#endif
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
        case '?':
            /* getopt_long has already printed an error message here */
            fprintf(stderr, "Run with -h to get usage info.\n");
            exit(1);
            break;
        case 'j':
            opt_json = 1;
            break;
        case 'h':
            print_usage();
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
        fprintf(stderr, "Error initializing USB!\n");
        exit(1);
    }

#if defined(__linux__) && (LIBUSB_API_VERSION >= 0x01000107)
    if (opt_sysdev) {
        sys_fd = open(opt_sysdev, O_RDWR);
        if (sys_fd < 0) {
            fprintf(stderr, "Cannot open system node!\n");
            rc = 1;
            goto cleanup;
        }
        rc = libusb_wrap_sys_device(NULL, sys_fd, &sys_devh);
        if (rc != 0) {
            fprintf(stderr,
                    "Cannot use %s as USB hub device, failed to wrap system node!\n",
                    opt_sysdev);
            rc = 1;
            goto cleanup;
        }
        usb_devs = calloc(2, sizeof *usb_devs);
        if (!usb_devs) {
            fprintf(stderr, "Out of memory\n");
            rc = 1;
            goto cleanup;
        }
        usb_devs[0] = libusb_get_device(sys_devh);
    } else {
        rc = libusb_get_device_list(NULL, &usb_devs);
    }
#else
    rc = libusb_get_device_list(NULL, &usb_devs);
#endif
    if (rc < 0) {
        fprintf(stderr, "Cannot enumerate USB devices!\n");
        rc = 1;
        goto cleanup;
    }

    is_rpi_4b = check_computer_model("Raspberry Pi 4 Model B");
    is_rpi_5  = check_computer_model("Raspberry Pi 5");

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

    if (hub_phys_count > 1 && opt_action != POWER_KEEP) {
        fprintf(stderr,
            "Error: changing port state for multiple hubs at once is not supported.\n"
            "Use -l to limit operation to one hub!\n"
        );
        exit(1);
    }

    /* For collecting hub JSON strings */
    char *hub_jsons[MAX_HUBS];
    int hub_json_count = 0;

    /* If no action is specified, just print status */
    if (opt_action == POWER_KEEP) {
        for (int i = 0; i < hub_count; i++) {
            if (hubs[i].actionable == 0)
                continue;

            if (opt_json) {
                hub_jsons[hub_json_count++] = create_hub_json(&hubs[i], opt_ports);
            } else {
                printf("Current status for hub %s [%s]\n",
                    hubs[i].location, hubs[i].ds.description);
                print_port_status(&hubs[i], opt_ports);
            }
        }
    } else {
        /* Main action loop (only runs if an action is specified) */
        int k;
        for (k = 0; k < 2; k++) {
            if (k == 0 && opt_action == POWER_ON)
                continue;
            if (k == 1 && opt_action == POWER_OFF)
                continue;
            /* if toggle requested, do it only once when k == 0 */
            if (k == 1 && opt_action == POWER_TOGGLE)
                continue;

            for (int i=0; i<hub_count; i++) {
                if (hubs[i].actionable == 0)
                    continue;

                char *hub_json_str = NULL;
                if (opt_json && opt_action == POWER_KEEP) {
                    /* Only create hub JSON for status queries, not power actions */
                    hub_json_str = create_hub_json(&hubs[i], opt_ports);
                } else if (!opt_json) {
                    printf("Current status for hub %s [%s]\n",
                        hubs[i].location, hubs[i].ds.description);
                    print_port_status(&hubs[i], opt_ports);
                } else if (opt_json && k == 0) {
                    /* For power actions in JSON mode, output initial hub status */
                    /* Example using the new array-based API */
                    mkjson_arg status_args[] = {
                        { MKJSON_STRING, "event", .value.str_val = "hub_status" },
                        { MKJSON_STRING, "hub", .value.str_val = hubs[i].location },
                        { MKJSON_STRING, "description", .value.str_val = hubs[i].ds.description },
                        { 0 } /* Null terminator */
                    };
                    char *status_json = mkjson_array_pretty(MKJSON_OBJ, status_args, 2);
                    printf("%s\n", status_json);
                    free(status_json);
                }

                struct libusb_device_handle *devh = NULL;
                rc = libusb_open(hubs[i].dev, &devh);
                if (rc == 0) {
                    /* will operate on these ports */
                    int ports = ((1 << hubs[i].nports) - 1) & opt_ports;
                    int should_be_on = k;
                    if (opt_action == POWER_FLASH) {
                        should_be_on = !should_be_on;
                    }

                    for (int port=1; port <= hubs[i].nports; port++) {
                        if ((1 << (port-1)) & ports) {
                            int port_status = get_port_status(devh, port);
                            int power_mask = hubs[i].super_speed ? USB_SS_PORT_STAT_POWER : USB_PORT_STAT_POWER;
                            int is_on = (port_status & power_mask) != 0;

                            if (opt_action == POWER_TOGGLE) {
                                should_be_on = !is_on;
                            }

                            if (is_on != should_be_on) {
                                rc = set_port_status(devh, &hubs[i], port, should_be_on);
                                if (opt_json && rc >= 0) {
                                    /* Output JSON event for power state change */
                                    mkjson_arg event_args[] = {
                                        { MKJSON_STRING, "event", .value.str_val = "power_change" },
                                        { MKJSON_STRING, "hub", .value.str_val = hubs[i].location },
                                        { MKJSON_INT, "port", .value.int_val = port },
                                        { MKJSON_STRING, "action", .value.str_val = should_be_on ? "on" : "off" },
                                        { MKJSON_BOOL, "from_state", .value.bool_val = is_on },
                                        { MKJSON_BOOL, "to_state", .value.bool_val = should_be_on },
                                        { MKJSON_BOOL, "success", .value.bool_val = rc >= 0 },
                                        { 0 }
                                    };
                                    char *event_json = mkjson_array_pretty(MKJSON_OBJ, event_args, 2);
                                    printf("%s\n", event_json);
                                    free(event_json);
                                }
                            }
                        }
                    }
                    /* USB3 hubs need extra delay to actually turn off: */
                    if (k==0 && hubs[i].super_speed)
                        sleep_ms(150);

                    if (!opt_json) {
                        printf("Sent power %s request\n", should_be_on ? "on" : "off");
                        printf("New status for hub %s [%s]\n",
                            hubs[i].location, hubs[i].ds.description);
                        print_port_status(&hubs[i], opt_ports);
                    }

                    if (k == 1 && opt_reset == 1) {
                        if (!opt_json) {
                            printf("Resetting hub...\n");
                        }
                        rc = libusb_reset_device(devh);
                        if (!opt_json) {
                            if (rc < 0) {
                                perror("Reset failed!\n");
                            } else {
                                printf("Reset successful!\n");
                            }
                        } else {
                            /* Output JSON event for hub reset */
                            mkjson_arg reset_args[] = {
                                { MKJSON_STRING, "event", .value.str_val = "hub_reset" },
                                { MKJSON_STRING, "hub", .value.str_val = hubs[i].location },
                                { MKJSON_BOOL, "success", .value.bool_val = rc >= 0 },
                                { MKJSON_STRING, "status", .value.str_val = rc < 0 ? "failed" : "successful" },
                                { 0 }
                            };
                            char *reset_json = mkjson_array_pretty(MKJSON_OBJ, reset_args, 2);
                            printf("%s\n", reset_json);
                            free(reset_json);
                        }
                    }
                }
                libusb_close(devh);

                if (opt_json && hub_json_str) {
                    hub_jsons[hub_json_count++] = hub_json_str;
                }
            }
            /* Handle delay between power off and power on for cycle/flash */
            if (k == 0 && (opt_action == POWER_CYCLE || opt_action == POWER_FLASH)) {
                if (opt_json) {
                    /* Output JSON event for delay */
                    mkjson_arg delay_args[] = {
                        { MKJSON_STRING, "event", .value.str_val = "delay" },
                        { MKJSON_STRING, "reason", .value.str_val = opt_action == POWER_CYCLE ? "power_cycle" : "power_flash" },
                        { MKJSON_DOUBLE, "duration_seconds", .value.dbl_val = opt_delay },
                        { 0 }
                    };
                    char *delay_json = mkjson_array_pretty(MKJSON_OBJ, delay_args, 2);
                    printf("%s\n", delay_json);
                    free(delay_json);
                }
                sleep_ms((int)(opt_delay * 1000));
            }
        }
    }

    if (opt_json && opt_action == POWER_KEEP) {
        /* Only output hub status array when no power action is performed */
        /* For power actions, we output events in real-time instead */
        char *hubs_array;
        if (hub_json_count == 0) {
            mkjson_arg empty_args[] = { { 0 } };
            hubs_array = mkjson_array_pretty(MKJSON_ARR, empty_args, 2);
        } else {
            /* Calculate total size needed */
            int total_size = 3; /* "[]" + null terminator */
            for (int i = 0; i < hub_json_count; i++) {
                total_size += strlen(hub_jsons[i]);
                if (i > 0) total_size += 2; /* ", " */
            }

            hubs_array = malloc(total_size);
            if (!hubs_array) {
                for (int i = 0; i < hub_json_count; i++) {
                    free(hub_jsons[i]);
                }
                return rc;
            }

            char* ptr = hubs_array;
            int remaining = total_size;

            int written = snprintf(ptr, remaining, "[");
            ptr += written;
            remaining -= written;

            for (int i = 0; i < hub_json_count; i++) {
                written = snprintf(ptr, remaining, "%s%s", i > 0 ? ", " : "", hub_jsons[i]);
                ptr += written;
                remaining -= written;
                free(hub_jsons[i]);
            }

            snprintf(ptr, remaining, "]");
        }

        /* Create the final JSON object */
        mkjson_arg final_args[] = {
            { MKJSON_JSON, "hubs", .value.str_val = hubs_array },
            { 0 }
        };
        char *json_str = mkjson_array_pretty(MKJSON_OBJ, final_args, 2);

        printf("%s\n", json_str);
        free(json_str);
        free(hubs_array);
    }

    rc = 0;
cleanup:
#if defined(__linux__) && (LIBUSB_API_VERSION >= 0x01000107)
    if (opt_sysdev && sys_fd >= 0) {
        if (sys_devh)
            libusb_close(sys_devh);
        close(sys_fd);
        free(usb_devs);
    } else if (usb_devs)
        libusb_free_device_list(usb_devs, 1);
#else
    if (usb_devs)
        libusb_free_device_list(usb_devs, 1);
#endif
    usb_devs = NULL;
    libusb_exit(NULL);
    return rc;
}
