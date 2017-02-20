/*
 * Copyright (c) 2009-2016 Vadim Mikhailov
 *
 * Utility to turn USB port power on/off
 * for USB hubs that support per-port power switching.
 *
 * This file can be distributed under the terms and conditions of the
 * GNU General Public License version 2.
 *
 */

#define PROGRAM_VERSION "1.5"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

#include <libusb-1.0/libusb.h>

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

/* Array of USB hubs we are going to operate on */
#define MAX_HUBS 64
static struct hub_info hubs[MAX_HUBS];
static int hub_count = 0;

/* default options */
static char opt_vendor[16]   = "";
static char opt_location[16] = "";     /* Hub location a-b.c.d */
static int opt_internal = 0;           /* Allow scanning internal hubs */
static int opt_ports  = ALL_HUB_PORTS; /* Bitmask of ports to operate on */
static int opt_action = POWER_KEEP;
static int opt_delay  = 2;
static int opt_repeat = 1;
static int opt_wait   = 20; /* wait before repeating in ms */

static const struct option long_options[] = {
    { "loc",      required_argument, NULL, 'l' },
    { "vendor",   required_argument, NULL, 'n' },
    { "internal", no_argument,       NULL, 'i' },
    { "ports",    required_argument, NULL, 'p' },
    { "action",   required_argument, NULL, 'a' },
    { "delay",    required_argument, NULL, 'd' },
    { "repeat",   required_argument, NULL, 'r' },
    { "wait",     required_argument, NULL, 'w' },
    { "version",  no_argument,       NULL, 'v' },
    { "help",     no_argument,       NULL, 'h' },
    { 0,          0,                 NULL, 0   },
};

int print_usage()
{
    printf(
        "uhubctl v%s: utility to control USB port power for smart hubs.\n"
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
        "--wait,     -w - wait before repeat power off [%d ms].\n"
        "--internal, -i - include internal hubs  [off].\n"
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
/* checks if hub is smart hub
 * use min_current above 0 to only consider external hubs
 * (external hubs have non-zero bHubContrCurrent)
 * return value is number of hub ports
 * 0 means hub is not smart
 * -1 means there is access error
 */

int is_smart_hub(struct libusb_device *dev, int min_current)
{
    int rc = 0;
    int len = 0;
    struct libusb_device_handle *devh = NULL;
    unsigned char buf[256] = {0};
    struct usb_hub_descriptor *uhd =
                (struct usb_hub_descriptor *)buf;
    int minlen = sizeof(struct usb_hub_descriptor) - 1;
    struct libusb_device_descriptor desc;
    rc = libusb_get_device_descriptor(dev, &desc);
    if (desc.bDeviceClass != LIBUSB_CLASS_HUB)
        return 0;
    rc = libusb_open(dev, &devh);
    if (rc == 0) {
        len = libusb_control_transfer(devh,
            LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS
                               | LIBUSB_RECIPIENT_DEVICE, /* hub status */
            LIBUSB_REQUEST_GET_DESCRIPTOR,
            LIBUSB_DT_HUB << 8,
            0,
            buf, sizeof(buf),
            USB_CTRL_GET_TIMEOUT
        );
        if (len >= minlen) {
            /* Logical Power Switching Mode */
            int lpsm = uhd->wHubCharacteristics[0] & HUB_CHAR_LPSM;
            /* Over-Current Protection Mode */
            int ocpm = uhd->wHubCharacteristics[0] & HUB_CHAR_OCPM;
            /* Both LPSM and OCPM must be supported per-port: */
            if ((lpsm == HUB_CHAR_INDV_PORT_LPSM) &&
                (ocpm == HUB_CHAR_INDV_PORT_OCPM))
            {
                rc = uhd->bNbrPorts;
                /* Internal hubs have zero bHubContrCurrent.
                 * Ignore them if requested:
                 */
                if (min_current > 0 && uhd->bHubContrCurrent < min_current) {
                    rc = -1;
                }
            }
        } else {
            rc = len;
        }
        libusb_close(devh);
    }
    return rc;
}

/* Assuming that devh is opened device handle for USB hub,
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

/* show status for hub ports
 * nports is number of hub ports
 * portmask is bitmap of ports to display
 * if portmask is 0, show all ports
 */
static int hub_port_status(struct libusb_device * dev, int nports, int portmask)
{
    int port_status;
    struct libusb_device_handle * devh = NULL;
    int rc = 0;
    rc = libusb_open(dev, &devh);
    if (rc == 0) {
        int port;
        for (port = 1; port <= nports; port++) {
            if (portmask > 0 && (portmask & (1 << (port-1))) == 0) continue;

            port_status = get_port_status(devh, port);
            if (port_status == -1) {
                fprintf(stderr,
                    "cannot read port %d status, %s (%d)\n",
                    port, strerror(errno), errno);
                break;
            }

            printf("   Port %d: %04x", port, port_status);
            printf("%s%s%s%s%s%s%s%s%s%s%s\n",
                port_status & USB_PORT_STAT_INDICATOR    ? " indicator" : "",
                port_status & USB_PORT_STAT_TEST         ? " test"      : "",
                port_status & USB_PORT_STAT_HIGH_SPEED   ? " highspeed" : "",
                port_status & USB_PORT_STAT_LOW_SPEED    ? " lowspeed"  : "",
                port_status & USB_PORT_STAT_POWER        ? " power"     : "",
                port_status & USB_PORT_STAT_RESET        ? " reset"     : "",
                port_status & USB_PORT_STAT_OVERCURRENT  ? " oc"        : "",
                port_status & USB_PORT_STAT_SUSPEND      ? " suspend"   : "",
                port_status & USB_PORT_STAT_ENABLE       ? " enable"    : "",
                port_status & USB_PORT_STAT_CONNECTION   ? " connect"   : "",
                port_status == 0                         ? " off"       : ""
            );
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
        int nports = is_smart_hub(dev, opt_internal ? 0 : 1);
        if (nports < 0) {
            perm_ok = 0; /* USB permission issue? */
        }
        if (nports > 0) { /* smart hub */
            if (hub_count < MAX_HUBS) {
                hubs[hub_count].dev    = dev;
                hubs[hub_count].nports = nports;

                /* Convert bus and ports array into USB location string */
                snprintf(
                    hubs[hub_count].vendor,
                    sizeof(hubs[hub_count].vendor),
                    "%04x:%04x",
                    desc.idVendor, desc.idProduct
                );

                int bus = libusb_get_bus_number(dev);
                sprintf(hubs[hub_count].location, "%d", bus);
                int pcount = libusb_get_port_path(NULL, dev, port_numbers, MAX_HUB_CHAIN);
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
        c = getopt_long(argc, argv, "l:n:a:p:d:r:w:hvi",
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
            opt_internal = 1;
            break;
        case 'n':
            strncpy(opt_vendor, optarg, sizeof(opt_vendor));
            break;
        case 'i':
            strncpy(opt_vendor, "", sizeof(opt_vendor));
            opt_internal = 1; /* enable internal hubs if location was specified */
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
            "Warning: changing port state for multiple hubs at once.\n"
            "Use -l to limit operation to one hub!\n"
        );
    }
    int i;
    for (i=0; i<hub_count; i++) {
        printf("Current status for hub %s, vendor %s, %d ports\n",
            hubs[i].location, hubs[i].vendor, hubs[i].nports
        );
        hub_port_status(hubs[i].dev, hubs[i].nports, opt_ports);
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
                        if (k == 0 && !(port_status & USB_PORT_STAT_POWER))
                            continue;
                        if (k == 1 && (port_status & USB_PORT_STAT_POWER))
                            continue;
                        int repeat = 1;
                        if (k == 0)
                            repeat = opt_repeat;
                        if (!(port_status & ~USB_PORT_STAT_POWER))
                            repeat = 1;
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
                                usleep(opt_wait * 1000);
                            }
                        }
                    }
                }
                if (k==0 && opt_action == POWER_CYCLE)
                    sleep(opt_delay);
                printf("Sent power %s request\n",
                    request == LIBUSB_REQUEST_CLEAR_FEATURE ? "off" : "on"
                );
                printf("New status for hub %s, vendor %s, %d ports\n",
                    hubs[i].location, hubs[i].vendor, hubs[i].nports
                );
                hub_port_status(hubs[i].dev, hubs[i].nports, opt_ports);
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
