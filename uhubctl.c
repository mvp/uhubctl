/*
 * Copyright (c) 2009-2018 Vadim Mikhailov
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

#if defined(__FreeBSD__) || defined(_WIN32)
#include <libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif

#if defined(__APPLE__) /* snprintf is not available in pure C mode */
int snprintf(char * __restrict __str, size_t __size, const char * __restrict __format, ...) __printflike(3, 4);
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

/*
 * Additions to wPortStatus bit field from USB 3.0
 * See USB 3.0 spec Table 10-10
 */
#define USB_PORT_STAT_LINK_STATE	0x01e0
#define USB_SS_PORT_STAT_POWER		0x0200
#define USB_SS_PORT_STAT_SPEED		0x1c00
#define USB_PORT_STAT_SPEED_5GBPS	0x0000
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

struct hub_info {
    struct libusb_device *dev;
    int bcd_usb;
    int nports;
    int ppps;
    char vendor[16];
    char location[32];
    char description[256];
};

/* Array of USB hubs we are going to operate on */
#define MAX_HUBS 64
static struct hub_info hubs[MAX_HUBS];
static int hub_count = 0;

/* default options */
static char opt_vendor[16]   = "";
static char opt_location[16] = "";     /* Hub location a-b.c.d */
static int opt_ports  = ALL_HUB_PORTS; /* Bitmask of ports to operate on */
static int opt_action = POWER_KEEP;
static int opt_delay  = 2;
static int opt_repeat = 1;
static int opt_wait   = 20; /* wait before repeating in ms */
static int opt_reset  = 0; /* reset hub after operation(s) */

static const struct option long_options[] = {
    { "loc",      required_argument, NULL, 'l' },
    { "vendor",   required_argument, NULL, 'n' },
    { "ports",    required_argument, NULL, 'p' },
    { "action",   required_argument, NULL, 'a' },
    { "delay",    required_argument, NULL, 'd' },
    { "repeat",   required_argument, NULL, 'r' },
    { "wait",     required_argument, NULL, 'w' },
    { "reset",    no_argument,       NULL, 'R' },
    { "version",  no_argument,       NULL, 'v' },
    { "help",     no_argument,       NULL, 'h' },
    { 0,          0,                 NULL, 0   },
};


int print_usage()
{
    printf(
        "uhubctl %s: utility to control USB port power for smart hubs.\n"
        "Usage: uhubctl [options]\n"
        "Without options, show status for all smart hubs.\n"
        "\n"
        "Options [defaults in brackets]:\n"
        "--action,   -a - action to off/on/cycle (0/1/2) for affected ports.\n"
        "--ports,    -p - ports to operate on    [all hub ports].\n"
        "--loc,      -l - limit hub by location  [all smart hubs].\n"
        "--vendor,   -n - limit hub by vendor id [%s] (partial ok).\n"
        "--delay,    -d - delay for cycle action [%d sec].\n"
        "--repeat,   -r - repeat power off count [%d] (some devices need it to turn off).\n"
        "--reset,    -R - reset hub after each power-on action, causing all devices to reassociate.\n"
        "--wait,     -w - wait before repeat power off [%d ms].\n"
        "--version,  -v - print program version.\n"
        "--help,     -h - print this text.\n"
        "\n"
        "Send bugs and requests to: https://github.com/mvp/uhubctl\n",
        PROGRAM_VERSION,
        strlen(opt_vendor) ? opt_vendor : "any",
        opt_delay,
        opt_repeat,
        opt_wait
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
 * get USB hub properties.
 * most hub_info fields are filled, except for description.
 * returns 0 for success and error code for failure.
 */

int get_hub_info(struct libusb_device *dev, struct hub_info *info)
{
    int rc = 0;
    int len = 0;
    struct libusb_device_handle *devh = NULL;
    unsigned char buf[LIBUSB_DT_HUB_NONVAR_SIZE + 2 + 3] = {0};
    struct usb_hub_descriptor *uhd =
                (struct usb_hub_descriptor *)buf;
    int minlen = LIBUSB_DT_HUB_NONVAR_SIZE + 2;
    struct libusb_device_descriptor desc;
    rc = libusb_get_device_descriptor(dev, &desc);
    if (rc)
        return rc;
    if (desc.bDeviceClass != LIBUSB_CLASS_HUB)
        return LIBUSB_ERROR_INVALID_PARAM;
    int bcd_usb = libusb_le16_to_cpu(desc.bcdUSB);
    int desc_type = (bcd_usb >= 0x300) ? LIBUSB_DT_SUPERSPEED_HUB
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
            info->nports  = uhd->bNbrPorts;
            info->ppps    = 0;
            /* Logical Power Switching Mode */
            int lpsm = uhd->wHubCharacteristics[0] & HUB_CHAR_LPSM;
            /* Over-Current Protection Mode */
            int ocpm = uhd->wHubCharacteristics[0] & HUB_CHAR_OCPM;
            /* LPSM must be supported per-port, and OCPM per port or ganged */
            if ((lpsm == HUB_CHAR_INDV_PORT_LPSM) &&
                (ocpm == HUB_CHAR_INDV_PORT_OCPM ||
                 ocpm == HUB_CHAR_COMMON_OCPM))
            {
                info->ppps = 1;
            }
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
 * Get USB device description as a string.
 *
 * It will use following format:
 *
 *    "<vid:pid> <vendor> <product> <serial>, <USB x.yz, N ports>"
 *
 * vid:pid will be always present, but vendor, product or serial
 * may be skipped if they are empty or not enough permissions to read them.
 * <USB x.yz, N ports> will be present only for USB hubs.
 *
 * returns 0 for success and error code for failure.
 * in case of failure description buffer is not altered.
 */

static int get_device_description(struct libusb_device * dev, char* description, int desc_len)
{
    int rc;
    int id_vendor  = 0;
    int id_product = 0;
    char vendor[64]  = "";
    char product[64] = "";
    char serial[64]  = "";
    char ports[64]   = "";
    struct libusb_device_descriptor desc;
    struct libusb_device_handle *devh = NULL;
    rc = libusb_get_device_descriptor(dev, &desc);
    if (rc)
        return rc;
    id_vendor  = libusb_le16_to_cpu(desc.idVendor);
    id_product = libusb_le16_to_cpu(desc.idProduct);
    rc = libusb_open(dev, &devh);
    if (rc == 0) {
        if (desc.iManufacturer) {
            libusb_get_string_descriptor_ascii(devh,
                desc.iManufacturer, (unsigned char*)vendor, sizeof(vendor));
            rtrim(vendor);
        }
        if (desc.iProduct) {
            libusb_get_string_descriptor_ascii(devh,
                desc.iProduct, (unsigned char*)product, sizeof(product));
            rtrim(product);
        }
        if (desc.iSerialNumber) {
            libusb_get_string_descriptor_ascii(devh,
                desc.iSerialNumber, (unsigned char*)serial, sizeof(serial));
            rtrim(serial);
        }
        if (desc.bDeviceClass == LIBUSB_CLASS_HUB) {
            struct hub_info info;
            rc = get_hub_info(dev, &info);
            if (rc == 0) {
                snprintf(ports, sizeof(ports), ", USB %x.%02x, %d ports",
                   info.bcd_usb >> 8, info.bcd_usb & 0xFF, info.nports);
            }
        }
        libusb_close(devh);
    }
    snprintf(description, desc_len,
        "%04x:%04x%s%s%s%s%s%s%s",
        id_vendor, id_product,
        vendor[0]  ? " " : "", vendor,
        product[0] ? " " : "", product,
        serial[0]  ? " " : "", serial,
        ports
    );
    return 0;
}


/*
 * show status for hub ports
 * portmask is bitmap of ports to display
 * if portmask is 0, show all ports
 */

static int hub_port_status(struct hub_info * hub, int portmask)
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

            char description[256] = "";
            struct libusb_device * udev;
            int i = 0;
            while ((udev = usb_devs[i++]) != NULL) {
                if (libusb_get_parent(udev)      == dev &&
                    libusb_get_port_number(udev) == port)
                {
                    rc = get_device_description(udev, description, sizeof(description));
                    if (rc == 0)
                        break;
                }
            }

            if (hub->bcd_usb < 0x300) {
                if (port_status & USB_PORT_STAT_INDICATOR)      printf(" indicator");
                if (port_status & USB_PORT_STAT_TEST)           printf(" test");
                if (port_status & USB_PORT_STAT_HIGH_SPEED)     printf(" highspeed");
                if (port_status & USB_PORT_STAT_LOW_SPEED)      printf(" lowspeed");
                if (port_status & USB_PORT_STAT_POWER)          printf(" power");
                if (port_status & USB_PORT_STAT_SUSPEND)        printf(" suspend");
            } else {
                int link_state = port_status & USB_PORT_STAT_LINK_STATE;
                if ((port_status & USB_SS_PORT_STAT_SPEED)
                     == USB_PORT_STAT_SPEED_5GBPS)
                {
                    printf(" 5gbps");
                }
                if (port_status & USB_SS_PORT_STAT_POWER)       printf(" power");
                if (link_state == USB_SS_PORT_LS_U0)            printf(" U0");
                if (link_state == USB_SS_PORT_LS_U1)            printf(" U1");
                if (link_state == USB_SS_PORT_LS_U2)            printf(" U2");
                if (link_state == USB_SS_PORT_LS_U3)            printf(" U3");
                if (link_state == USB_SS_PORT_LS_SS_DISABLED)   printf(" SS.Disabled");
                if (link_state == USB_SS_PORT_LS_RX_DETECT)     printf(" Rx.Detect");
                if (link_state == USB_SS_PORT_LS_SS_INACTIVE)   printf(" SS.Inactive");
                if (link_state == USB_SS_PORT_LS_POLLING)       printf(" Polling");
                if (link_state == USB_SS_PORT_LS_RECOVERY)      printf(" Recovery");
                if (link_state == USB_SS_PORT_LS_HOT_RESET)     printf(" HotReset");
                if (link_state == USB_SS_PORT_LS_COMP_MOD)      printf(" Compliance");
                if (link_state == USB_SS_PORT_LS_LOOPBACK)      printf(" Loopback");
            }
            if (port_status & USB_PORT_STAT_RESET)          printf(" reset");
            if (port_status & USB_PORT_STAT_OVERCURRENT)    printf(" oc");
            if (port_status & USB_PORT_STAT_ENABLE)         printf(" enable");
            if (port_status & USB_PORT_STAT_CONNECTION)     printf(" connect");
            if (port_status == 0)                           printf(" off");

            if (port_status & USB_PORT_STAT_CONNECTION)     printf(" [%s]", description);

            printf("\n");
        }
        libusb_close(devh);
    }
    return 0;
}


/*
 *  Find all smart hubs that we are going to work on and fill hubs[] array.
 *  This applies possible constraints like location or vendor.
 *  Returns count of found hubs or negative error code.
 */

static int usb_find_hubs()
{
    struct libusb_device *dev;
    unsigned char port_numbers[MAX_HUB_CHAIN] = {0};
    int perm_ok = 1;
    int rc = 0;
    int i = 0;
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
        }
        if (info.ppps) { /* PPPS is supported */
            if (hub_count < MAX_HUBS) {
                hubs[hub_count].dev     = dev;
                hubs[hub_count].nports  = info.nports;
                hubs[hub_count].bcd_usb = info.bcd_usb;
                hubs[hub_count].ppps    = info.ppps;

                snprintf(
                    hubs[hub_count].vendor, sizeof(hubs[hub_count].vendor),
                    "%04x:%04x",
                    libusb_le16_to_cpu(desc.idVendor),
                    libusb_le16_to_cpu(desc.idProduct)
                );

                get_device_description(dev, hubs[hub_count].description, sizeof(hubs[hub_count].description));

                /* Convert bus and ports array into USB location string */
                int bus = libusb_get_bus_number(dev);
                snprintf(hubs[hub_count].location, sizeof(hubs[hub_count].location), "%d", bus);
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000102)
                /*
                 * libusb_get_port_path is deprecated since libusb v1.0.16,
                 * therefore use libusb_get_port_numbers when supported
                 */
                int pcount = libusb_get_port_numbers(dev, port_numbers, MAX_HUB_CHAIN);
#else
                int pcount = libusb_get_port_path(NULL, dev, port_numbers, MAX_HUB_CHAIN);
#endif
                int k;
                for (k=0; k<pcount; k++) {
                    char s[8];
                    snprintf(s, sizeof(s), "%s%d", k==0 ? "-" : ".", port_numbers[k]);
                    strcat(hubs[hub_count].location, s);
                }

                /* apply location and other filters: */
                if (strlen(opt_location)>0 && strcasecmp(opt_location, hubs[hub_count].location))
                    continue;
                if (strlen(opt_vendor)>0   && strncasecmp(opt_vendor,  hubs[hub_count].vendor, strlen(opt_vendor)))
                    continue;

                hub_count++;
            }
        }
    }
    if (perm_ok == 0 && hub_count == 0) {
        return LIBUSB_ERROR_ACCESS;
    }
    return hub_count;
}


int main(int argc, char *argv[])
{
    int rc;
    int c = 0;
    int option_index = 0;

    for (;;) {
        c = getopt_long(argc, argv, "l:n:a:p:d:r:w:hvR",
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
        case 'n':
            strncpy(opt_vendor, optarg, sizeof(opt_vendor));
            break;
        case 'p':
            if (!strcasecmp(optarg, "all")) { /* all ports is the default */
                break;
            }
            if (strlen(optarg)) {
                /* parse port list */
                opt_ports = 0;
                size_t i;
                for (i=0; i<strlen(optarg); i++) {
                    if (!isdigit(optarg[i]) || optarg[i] == '0') {
                        printf("%s must be list of ports 1 to %d\n", optarg, MAX_HUB_PORTS);
                    }
                    int d = optarg[i]-'1';
                    opt_ports |= (1 << d);
                }
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
            break;
        case 'd':
            opt_delay = atoi(optarg);
            break;
        case 'r':
            opt_repeat = atoi(optarg);
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
            "No compatible smart hubs detected%s%s!\n"
            "Run with -h to get usage info.\n",
            strlen(opt_location) ? " at location " : "",
            opt_location
        );
#ifdef __gnu_linux__
        if (rc < 0) {
            fprintf(stderr,
                "There were permission problems while accessing USB.\n"
                "To fix this, run this tool as root using 'sudo uhubctl',\n"
                "or add one or more udev rules like below\n"
                "to file '/etc/udev/rules.d/52-usb.rules':\n"
                "SUBSYSTEM==\"usb\", ATTR{idVendor}==\"2001\", MODE=\"0666\"\n"
                "then run 'sudo udevadm trigger --attr-match=subsystem=usb'\n"
            );
        }
#endif
        rc = 1;
        goto cleanup;
    }

    if (hub_count > 1 && opt_action >= 0) {
        fprintf(stderr,
            "Error: changing port state for multiple hubs at once is not supported.\n"
            "Use -l to limit operation to one hub!\n"
        );
        exit(1);
    }
    int i;
    for (i=0; i<hub_count; i++) {
        printf("Current status for hub %s [%s]\n",
            hubs[i].location, hubs[i].description
        );
        hub_port_status(&hubs[i], opt_ports);
        if (opt_action == POWER_KEEP) { /* no action, show status */
            continue;
        }
        struct libusb_device_handle * devh = NULL;
        rc = libusb_open(hubs[i].dev, &devh);
        if (rc == 0) {
            /* will operate on these ports */
            int ports = ((1 << hubs[i].nports) - 1) & opt_ports;
            int k; /* k=0 for power OFF, k=1 for power ON */
            for (k=0; k<2; k++) { /* up to 2 power actions - off/on */
                if (k == 0 && opt_action == POWER_ON )
                    continue;
                if (k == 1 && opt_action == POWER_OFF)
                    continue;
                int request = (k == 0) ? LIBUSB_REQUEST_CLEAR_FEATURE
                                       : LIBUSB_REQUEST_SET_FEATURE;
                int port;
                for (port=1; port <= hubs[i].nports; port++) {
                    if ((1 << (port-1)) & ports) {
                        int port_status = get_port_status(devh, port);
                        if(hubs[i].bcd_usb < 0x300 )
                        {
                            if (k == 0 && !(port_status & USB_PORT_STAT_POWER))
                                continue;
                            if (k == 1 && (port_status & USB_PORT_STAT_POWER))
                                continue;
                        }
                        else
                        {
                            if (k == 0 && !(port_status & USB_SS_PORT_STAT_POWER))
                                continue;
                            if (k == 1 && (port_status & USB_SS_PORT_STAT_POWER))
                                continue;
                        }
                        int repeat = 1;
                        if (k == 0)
                            repeat = opt_repeat;
                        if(hubs[i].bcd_usb < 0x300 )
                        {
                            if (!(port_status & ~USB_PORT_STAT_POWER))
                                repeat = 1;
                        }
                        else
                        {
                            if (!(port_status & ~USB_SS_PORT_STAT_POWER))
                                repeat = 1;
                        }
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
                if (k==0 && opt_action == POWER_CYCLE)
                    sleep_ms(opt_delay * 1000);
                printf("Sent power %s request\n",
                    request == LIBUSB_REQUEST_CLEAR_FEATURE ? "off" : "on"
                );
                printf("New status for hub %s [%s]\n",
                    hubs[i].location, hubs[i].description
                );
                hub_port_status(&hubs[i], opt_ports);

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
    }
    rc = 0;
cleanup:
    if (usb_devs)
        libusb_free_device_list(usb_devs, 1);
    usb_devs = NULL;
    libusb_exit(NULL);
    return rc;
}
