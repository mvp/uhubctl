uhubctl
=======

`uhubctl` is utility to control USB power per-port on smart USB hubs.
Smart hub is defined as one that implements per-port power switching.

Original idea for this code was inspired by hub-ctrl.c by Niibe Yutaka:
https://www.gniibe.org/development/ac-power-control-by-USB-hub


Compatible USB hubs
===================

Note that very few hubs actually support per-port power switching.
Some of them are no longer manufactured and can be hard to find.

This is list of known compatible USB hubs:

| Manufacturer       | Product                                              | Ports | USB | VID:PID   | Release | EOL  |
|:-------------------|:-----------------------------------------------------|:------|:----|:----------|:--------|:-----|
| AmazonBasics       | HU3641V1 ([RPi issue](https://goo.gl/CLt46M))        | 4     | 3.0 |`2109:2811`| 2013    |      |
| AmazonBasics       | HU3770V1 ([RPi issue](https://goo.gl/CLt46M))        | 7     | 3.0 |`2109:2811`| 2013    |      |
| AmazonBasics       | HU9002V1SBL, HU9002V1ESL, HUC9002V1SBL, HUC9002V1EBL | 10    | 3.1 |`2109:2817`| 2018    |      |
| Apple              | Thunderbolt Display 27" (internal USB hub)           | 6     | 2.0 |           | 2011    | 2016 |
| Apple              | USB Keyboard With Numeric Pad (internal USB hub)     | 3     | 2.0 |           | 2011    |      |
| Asus               | Z87-PLUS Motherboard (onboard USB hub)               | 4     | 3.0 |           | 2013    | 2016 |
| B+B SmartWorx      | UHR204                                               | 4     | 2.0 |`0856:DB00`| 2013    |      |
| B+B SmartWorx      | USH304                                               | 4     | 3.0 |`04B4:6506`| 2017    |      |
| Basler             | 2000036234                                           | 4     | 3.0 |`0451:8046`| 2016    |      |
| Belkin             | F5U101                                               | 4     | 2.0 |`0451:2046`| 2005    | 2010 |
| Buffalo            | BSH4A05U3BK                                          | 4     | 3.0 |`05E3:0610`| 2015    |      |
| Bytecc             | BT-UH340                                             | 4     | 3.0 |`2109:8110`| 2010    |      |
| Circuitco          | Beagleboard-xM (internal USB hub)                    | 4     | 2.0 |`0424:9514`| 2010    |      |
| Club3D             | CSV-3242HD Dual Display Docking Station              | 4     | 3.0 |`2109:2811`| 2015    |      |
| CyberPower         | CP-H420P                                             | 4     | 2.0 |`0409:0059`| 2004    |      |
| Cypress            | CY4608 HX2VL development kit                         | 4     | 2.0 |`04B4:6570`| 2012    |      |
| D-Link             | DUB-H4 rev B (silver)                                | 4     | 2.0 |`05E3:0605`| 2005    | 2010 |
| D-Link             | DUB-H4 rev D,E (black). Note: rev A,C not supported  | 4     | 2.0 |`05E3:0608`| 2012    |      |
| D-Link             | DUB-H7 rev A (silver)                                | 7     | 2.0 |`2001:F103`| 2005    | 2010 |
| D-Link             | DUB-H7 rev D,E (black). Note: rev B,C not supported  | 7     | 2.0 |`05E3:0608`| 2012    |      |
| Dell               | P2416D 24" QHD Monitor                               | 4     | 2.0 |           | 2017    |      |
| Dell               | UltraSharp 1704FPT 17" LCD Monitor                   | 4     | 2.0 |`0424:A700`| 2005    | 2015 |
| Dell               | UltraSharp U2415 24" LCD Monitor                     | 5     | 3.0 |           | 2014    |      |
| Elecom             | U2H-G4S                                              | 4     | 2.0 |           | 2006    | 2011 |
| GlobalScale        | ESPRESSObin SBUD102 V5                               | 1     | 3.0 |`1D6B:0003`| 2017    |      |
| Hawking Technology | UH214                                                | 4     | 2.0 |           | 2003    | 2008 |
| IOI                | U3H415E1                                             | 4     | 3.0 |           | 2012    |      |
| j5create           | JUH470 (works only in USB2 mode)                     | 3     | 3.0 |`05E3:0610`| 2014    |      |
| Juiced Systems     | 6HUB-01                                              | 7     | 3.0 |`0BDA:0411`| 2014    | 2018 |
| LG Electronics     | 38WK95C-W monitor                                    | 4     | 3.0 |`0451:8142`| 2018    |      |
| Lenovo             | ThinkPad Ultra Docking Station (40A20090EU)          | 6     | 2.0 |`17EF:100F`| 2015    |      |
| Lenovo             | ThinkPad Ultra Docking Station (40AJ0135EU)          | 7     | 3.1 |`17EF:3070`| 2018    |      |
| Lenovo             | ThinkPad X200 Ultrabase 42X4963                      | 3     | 2.0 |`17EF:1005`| 2008    | 2011 |
| Lenovo             | ThinkPad X6 Ultrabase 42W3107                        | 4     | 2.0 |`17EF:1000`| 2006    | 2009 |
| Lindy              | USB serial converter 4 port                          | 4     | 1.1 |`058F:9254`| 2008    |      |
| Linksys            | USB2HUB4                                             | 4     | 2.0 |           | 2004    | 2010 |
| Maplin             | A08CQ                                                | 7     | 2.0 |`0409:0059`| 2008    | 2011 |
| Microchip          | EVB-USB2517                                          | 7     | 2.0 |           | 2008    |      |
| Moxa               | Uport-407                                            | 7     | 2.0 |`110A:0407`| 2009    |      |
| NVidia             | Jetson Nano B01 ([details](https://git.io/JJaFR))    | 4     | 3.0 |           | 2019    |      |
| Phidgets           | HUB0003_0                                            | 7     | 2.0 |`1A40:0201`| 2017    |      |
| Plugable           | USB3-HUB7BC                                          | 7     | 3.0 |`2109:0813`| 2015    |      |
| Plugable           | USB3-HUB7C                                           | 7     | 3.0 |`2109:0813`| 2015    |      |
| Plugable           | USB3-HUB7-81X                                        | 7     | 3.0 |`2109:0813`| 2012    |      |
| Raspberry Pi       | B+, 2B, 3B ([see below](#raspberry-pi-b2b3b))        | 4     | 2.0 |           | 2011    |      |
| Raspberry Pi       | 3B+        ([see below](#raspberry-pi-3b))           | 4     | 2.0 |`0424:2514`| 2018    |      |
| Raspberry Pi       | 4B         ([see below](#raspberry-pi-4b))           | 4     | 3.0 |`2109:3431`| 2019    |      |
| Renesas            | uPD720202 PCIe USB 3.0 host controller               | 2     | 3.0 |           | 2013    |      |
| Rosewill           | RHUB-210                                             | 4     | 2.0 |`0409:005A`| 2011    | 2014 |
| Sanwa Supply       | USB-HUB14GPH                                         | 4     | 1.1 |           | 2001    | 2003 |
| Seagate            | Backup Plus Hub STEL8000100                          | 2     | 3.0 |`0BC2:AB44`| 2016    |      |
| Sunix              | SHB4200MA                                            | 4     | 2.0 |`0409:0058`| 2006    | 2009 |
| Targus             | PAUH212U                                             | 7     | 2.0 |           | 2004    | 2009 |
| Texas Instruments  | TUSB4041PAPEVM                                       | 4     | 2.1 |`0451:8142`| 2015    |      |

This table is by no means complete.
If your hub works with `uhubctl`, but is not listed above, please report it
by opening new issue at https://github.com/mvp/uhubctl/issues,
so we can add it to supported table. In your report, please provide
exact product model and add output from `uhubctl`
and please test VBUS off support as described below in FAQ.

Note that quite a few modern motherboards have built-in root hubs that
do support this feature - you may not even need to buy any external hub.


USB 3.0 duality note
====================
If you have USB 3.0 hub connected to USB3 upstream port, it will be detected
as 2 independent virtual hubs: USB2 and USB3, and your USB devices will be connected
to USB2 or USB3 virtual hub depending on their capabilities and connection speed.
To control power for such hubs, it is necessary to turn off/on power on **both** USB2 and USB3
virtual hubs for power off/on changes to take effect. `uhubctl` will try to do this automatically
(unless you disable this behavior with option `-e`).

Unfortunately, while most hubs will cut off data USB connection, some may still not cut off VBUS to port,
which means connected phone may still continue to charge from port that is powered off by `uhubctl`.


Compiling
=========

This utility was tested to compile and work on Linux
(Ubuntu/Debian, Redhat/Fedora/CentOS, Arch Linux, Gentoo, openSUSE, Buildroot), FreeBSD, NetBSD and Mac OS X.

While `uhubctl` compiles on Windows, USB power switching does not work on Windows because `libusb`
is using `winusb.sys` driver, which according to Microsoft does not support
[necessary USB control requests](https://social.msdn.microsoft.com/Forums/sqlserver/en-US/f680b63f-ca4f-4e52-baa9-9e64f8eee101).
This may be fixed if `libusb` starts supporting different driver on Windows.

First, you need to install library libusb-1.0 (version 1.0.12 or later, 1.0.16 or later is recommended):

* Ubuntu: `sudo apt-get install libusb-1.0-0-dev`
* Redhat: `sudo yum install libusb1-devel`
* MacOSX: `brew install libusb`, or `sudo port install libusb-devel`
  > :warning: `libusb-1.0.23` is [broken](https://github.com/libusb/libusb/issues/707) on MacOS Catalina!
  You have to install `libusb-1.0.22` until [libusb issue 707](https://github.com/libusb/libusb/issues/707) is fixed,
  or use this workaround to force use of older Mojave build:

      brew uninstall --ignore-dependencies libusb
      brew install https://raw.githubusercontent.com/Homebrew/homebrew-core/5314f1d/Formula/libusb.rb

* FreeBSD: libusb is included by default
* NetBSD: `sudo pkgin install libusb1 gmake pkg-config`
* Windows: TBD?

To fetch uhubctl source:

    git clone https://github.com/mvp/uhubctl

To compile, simply run `make` - this will generate `uhubctl` binary.
Note that on some OS (e.g. FreeBSD/NetBSD) you need to use `gmake` instead to build.

Also, for Mac OS X you can install `uhubctl` with Homebrew custom tap:

```
brew tap mvp/uhubctl https://github.com/mvp/uhubctl
brew install --HEAD uhubctl
```

Usage
=====

You can control the power on a USB port(s) like this:

    uhubctl -a off -p 2

This means operate on default smart hub and turn power off (`-a off`, or `-a 0`)
on port 2 (`-p 2`). Supported actions are `off`/`on`/`cycle` (or `0`/`1`/`2`).
`cycle` means turn power off, wait some delay (configurable with `-d`) and turn it back on.
Ports can be comma separated list, and may use `-` for ranges e.g. `2`, or `2,4`, or `2-5`, or `1-2,5-8`.

> :warning: Turning off built-in USB ports may cut off your keyboard or mouse,
so be careful which ports you are turning off!

If you have more than one smart USB hub connected, you should choose
specific hub to control using `-l` (location) parameter.
To find hub locations, simply run `uhubctl` without any parameters.
Hub locations look like `b-x.y.z`, where `b` is USB bus number, and `x`, `y`, `z`...
are port numbers for all hubs in chain, starting from root hub for a given USB bus.
This address is semi-stable - it will not change if you unplug/replug (or turn off/on)
USB device into the same physical USB port (this method is also used in Linux kernel).


Linux USB permissions
=====================

On Linux, you should configure `udev` USB permissions (otherwise you will have to run it as root using `sudo uhubctl`).
To fix USB permissions, first run `sudo uhubctl` and note all `vid:pid` for hubs you need to control.
Then, add one or more udev rules like below to file `/etc/udev/rules.d/52-usb.rules` (replace with your vendor id):

    SUBSYSTEM=="usb", ATTR{idVendor}=="2001", MODE="0666"

If you don't like wide open mode `0666`, you can restrict access by group like this:

    SUBSYSTEM=="usb", ATTR{idVendor}=="2001", MODE="0664", GROUP="dialout"

and then add permitted users to `dialout` group:

    sudo usermod -a -G dialout $USER

For your `udev` rule changes to take effect, reboot or run:

    sudo udevadm trigger --attr-match=subsystem=usb



FAQ
===

#### _What is USB per-port power switching?_

According to USB 2.0 specification, USB hubs can advertise no power switching,
ganged (all ports at once) power switching or per-port (individual) power switching.
Note that `uhubctl` will only detect USB hubs which support per-port power switching.
You can find what kind of power switching your hardware supports by using `sudo lsusb -v`:

No power switching:

    wHubCharacteristic 0x000a
      No power switching (usb 1.0)
      Per-port overcurrent protection

Ganged power switching:

    wHubCharacteristic 0x0008
      Ganged power switching
      Per-port overcurrent protection

Per-port power switching:

    wHubCharacteristic 0x0009
      Per-port power switching
      Per-port overcurrent protection


#### _How do I check if my USB hub is supported by `uhubctl`?_

1. Run `sudo uhubctl`. If your hub is not listed, it is not supported.
   Alternatively, you can run `sudo lsusb -v` and check for
   `Per-port power switching` -  if you cannot see such line in lsusb output,
   hub is not supported.
2. Check for VBUS (voltage) off support: plug a phone, USB light
    or USB fan into USB port of your hub.
   Try using `uhubctl` to turn power off on that port, and check
   that phone stops charging, USB light stops shining or USB fan stops spinning.
   If VBUS doesn't turn off, your hub manufacturer did not include circuitry
   to actually cut power off. Such hub would still work
   to cut off USB data connection, but it cannot turn off power,
   and we do not consider this supported device.
3. If tests above were successful, please report your hub
   by opening new issue at https://github.com/mvp/uhubctl/issues,
   so we can add it to list of supported devices.


#### _USB devices are not removed after port power down on Linux_

After powering down USB port, udev does not get any event, so it keeps the device files around.
However, trying to access the device files will lead to an IO error.

This is Linux kernel issue. It may be eventually fixed in kernel, see more discussion [here](https://bit.ly/2JzczjZ).
Basically what happens here is that kernel USB driver knows about power off,
but doesn't send notification about it to udev.

You can use this workaround for this issue:

    sudo uhubctl -a off -l ${location} -p ${port}
    sudo udevadm trigger --action=remove /sys/bus/usb/devices/${location}.${port}/

Device file will be removed by udev, but USB device will be still visible in `lsusb`.
Note that path `/sys/bus/usb/devices/${location}.${port}` will only exist if device was detected on that port.
When you turn power back on, device should re-enumerate properly (no need to call `udevadm` again).

#### _Power comes back on after few seconds on Linux_

Some device drivers in kernel are surprised by USB device being turned off and automatically try to power it back on.

You can use option `-r N` where N is some number from 10 to 1000 to fix this -
`uhubctl` will try to turn power off many times in quick succession, and it should suppress that.
This may be eventually fixed in kernel, see more discussion [here](https://bit.ly/2JzczjZ).

If your device is USB mass storage, invoking `udisksctl` before calling `uhubctl`
might help to mitigate this issue:

    sudo udisksctl power-off --block-device /dev/disk/...`
    sudo uhubctl -a off ...


#### _Multiple 4-port hubs are detected, but I only have one 7-port hub connected_

Many hub manufacturers build their USB hubs using basic 4 port USB chips.
E.g. to make 7 port hub, they daisy-chain two 4 port hubs - 1 port is lost to daisy-chaining,
so it makes it 4+4-1=7 port hub. Similarly, 10 port hub could be built as 3 4-port hubs
daisy-chained together, which gives 4+4+4-2=10 usable ports.

Note that you should never try to change power state for ports used to daisy-chain internal hubs together.
Doing so will confuse internal hub circuitry and will cause unpredictable behavior.


#### _Raspberry Pi turns power off on all ports, not just the one I specified_

This is limitation of Raspberry Pi hardware design.

For reference, supported Raspberry Pi models have following internal USB topology:

##### Raspberry Pi B+,2B,3B

  * Single hub `1-1`, ports 2-5 ganged, all controlled by port `2`:

        uhubctl -l 1-1 -p 2 -a 0

    Trying to control ports `3`,`4`,`5` will not do anything.
    Port `1` controls power for Ethernet+WiFi.

##### Raspberry Pi 3B+

  * Main hub `1-1`, all 4 ports ganged, all controlled by port `2` (turns off secondary hub ports as well).
    Port `1` connects hub `1-1.1` below, ports `2` and `3` are wired outside, port `4` not wired.

        uhubctl -l 1-1 -p 2 -a 0

  * Secondary hub `1-1.1` (daisy-chained to main): 3 ports,
    port `1` is used for Ethernet+WiFi, and ports `2` and `3` are wired outside.


##### Raspberry Pi 4B

 > :warning: If your VL805 firmware is older than `00137ad` (check with `sudo rpi-eeprom-update`),
you have to [update firmware](https://www.raspberrypi.org/documentation/hardware/raspberrypi/booteeprom.md)
to make power switching work on RPi 4B.

  * USB2 hub `1`, 1 port, only connects hub `1-1` below.

  * USB2 hub `1-1`, 4 ports ganged, dual to USB3 hub `2` below:

        uhubctl -l 1-1 -a 0

  * USB3 hub `2`, 4 ports ganged, dual to USB2 hub `1-1` above:

        uhubctl -l 2 -a 0

  * USB2 hub `3`, 1 port, OTG controller:

        uhubctl -l 3 -p 1 -a 0


As a workaround, you can buy any external USB hub from supported list,
attach it to any USB port of Raspberry Pi, and control power on its ports independently.



Notable projects using uhubctl
==============================
| Project                                                  | Description                                           |
|:---------------------------------------------------------|:------------------------------------------------------|
| [Morse code USB light](https://git.io/fj1F4)             | Flash a message in Morse code with USB light          |
| [Webcam USB light](https://git.io/fj1FB)                 | Turn on/off LED when webcam is turned on/off          |
| [Cinema Lightbox](https://goo.gl/fjCvkz)                 | Turn on/off Cinema Lightbox from iOS Home app         |
| [Build Status Light](https://goo.gl/3GA82o)              | Create a build status light in under 10 minutes       |
| [Buildenlights](https://git.io/fj1FC)                    | GitLab/GitHub project build status as green/red light |
| [Weather Station](https://goo.gl/3b1FzC)                 | Reset Weather Station when it freezes                 |
| [sysmoQMOD](https://bit.ly/2VtWrVt)                      | Reset cellular modems when necessary                  |
| [Smog Sensor](https://bit.ly/2EMwgCk)                    | Raspberry Pi based smog sensor power reset            |
| [Terrible Cluster](https://goo.gl/XjiXFu)                | Power on/off Raspberry Pi cluster nodes as needed     |
| [Ideal Music Server](https://bit.ly/39MeVFQ)             | Turn off unused USB ports to improve audio quality    |
| [USB drives with no phantom load](https://goo.gl/qfrmGK) | Power USB drives only when needed to save power       |
| [USB drive data recovery](https://goo.gl/4MddLr)         | Recover data from failing USB hard drive              |
| [Control power to 3D printer](https://git.io/fh5Tr)      | OctoPrint web plugin for USB power control            |
| [USB fan for Raspberry Pi](https://bit.ly/2TRV6sM)       | Control USB fan to avoid Raspberry Pi overheating     |
| [Raspberry Pi Reboot Router](https://bit.ly/3aNbQqs)     | Automatically reboot router if internet isn't working |
| [Control USB Lamp With Voice](https://bit.ly/2VtW2SX)    | Voice Control of USB Lamp using Siri and Raspberry Pi |


Copyright
=========

Copyright (C) 2009-2020 Vadim Mikhailov

This file can be distributed under the terms and conditions of the
GNU General Public License version 2.
