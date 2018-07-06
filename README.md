uhubctl
=======

`uhubctl` is utility to control USB power per-port on smart USB hubs.
Smart hub is defined as one that implements per-port power switching.

Original idea for this code was inspired by hub-ctrl.c by Niibe Yutaka:
http://www.gniibe.org/development/ac-power-control-by-USB-hub


Compatible USB hubs
===================

Note that very few hubs actually support per-port power switching.
Some of them are no longer manufactured and can be hard to find.

This is list of known compatible USB hubs:

| Manufacturer       | Product                                              | Ports | USB | VID:PID   | Release | EOL  |
|:-------------------|:-----------------------------------------------------|:------|:----|:----------|:--------|:-----|
| AmazonBasics       | HU3641V1                                             | 4     | 3.0 |`2109:2811`| 2013    |      |
| AmazonBasics       | HU3770V1                                             | 7     | 3.0 |`2109:2811`| 2013    |      |
| Apple              | Thunderbolt Display 27" (internal USB hub)           | 6     | 2.0 |           | 2011    | 2016 |
| Apple              | USB Keyboard With Numeric Pad (internal USB hub)     | 3     | 2.0 |           | 2011    |      |
| Asus               | Z87-PLUS Motherboard (onboard USB hub)               | 4     | 3.0 |           | 2013    | 2016 |
| Anker              | 4-Port USB 3.0 HUB (Model No.: H4928-U3)             | 4     | 3.0 |`2109:0812`| 2014    | 2016 |
| B&B Electronics    | UHR204                                               | 4     | 2.0 |           | 2013    |      |
| Belkin             | F5U701-BLK                                           | 7     | 2.0 |           | 2008    | 2012 |
| Circuitco          | Beagleboard-xM (internal USB hub)                    | 4     | 2.0 |`0424:9514`| 2010    |      |
| CyberPower         | CP-H420P                                             | 4     | 2.0 |`0409:0059`| 2004    |      |
| Cypress            | CY4608 HX2VL development kit                         | 4     | 2.0 |`04B4:6570`| 2012    |      |
| D-Link             | DUB-H4 rev D1 (black edition)                        | 4     | 2.0 |`05E3:0608`| 2012    |      |
| D-Link             | DUB-H7 rev A  (silver edition)                       | 7     | 2.0 |`2001:F103`| 2005    | 2010 |
| D-Link             | DUB-H7 rev D1 (black edition)                        | 7     | 2.0 |`05E3:0608`| 2012    |      |
| Delock             | 87445                                                | 4     | 2.0 |`05E3:0608`| 2009    | 2013 |
| Elecom             | U2H-G4S                                              | 4     | 2.0 |           | 2006    | 2011 |
| Hawking Technology | UH214                                                | 4     | 2.0 |           | 2003    | 2008 |
| j5create           | JUH470 (works only in USB2 mode)                     | 3     | 3.0 |`05E3:0610`| 2014    |      |
| Lenovo             | ThinkPad EU Ultra Dockingstation (40A20090EU)        | 6     | 2.0 |`17EF:100F`| 2015    |      |
| Lenovo             | ThinkPad X200 Ultrabase 42X4963                      | 3     | 2.0 |`17EF:1005`| 2008    | 2011 |
| Lindy              | USB serial converter 4 port                          | 4     | 1.1 |`058F:9254`| 2008    |      |
| Linksys            | USB2HUB4                                             | 4     | 2.0 |           | 2004    | 2010 |
| Maplin             | A08CQ                                                | 7     | 2.0 |`0409:0059`| 2008    | 2011 |
| Microchip          | EVB-USB2517                                          | 7     | 2.0 |           | 2008    |      |
| Moxa               | Uport-407                                            | 4     | 2.0 |           | 2017    |      |
| Plugable           | USB3-HUB7BC                                          | 7     | 3.0 |`2109:0813`| 2015    |      |
| Plugable           | USB3-HUB7C                                           | 7     | 3.0 |`2109:0813`| 2015    |      |
| Plugable           | USB3-HUB7-81X                                        | 7     | 3.0 |`2109:0813`| 2012    |      |
| Plugable           | USB2-HUB10S                                          | 10    | 2.0 |           | 2010    |      |
| Raspberry Pi       | Model B+, 2 B, 3 B (port 2 only)                     | 4     | 2.0 |           | 2011    |      |
| Raspberry Pi       | Model 3 B+                                           | 6     | 2.0 |`0424:2514`| 2018    |      |
| Renesas            | uPD720202 PCIe USB 3.0 host controller               | 2     | 3.0 |           | 2013    |      |
| Rosewill           | RHUB-210                                             | 4     | 2.0 |`0409:005A`| 2011    | 2014 |
| Sanwa Supply       | USB-HUB14GPH                                         | 4     | 1.1 |           | 2001    | 2003 |
| Sunix              | SHB4200MA                                            | 4     | 2.0 |`0409:0058`| 2006    | 2009 |
| Targus             | PAUH212U                                             | 7     | 2.0 |           | 2004    | 2009 |

This table is by no means complete.
If your hub works with `uhubctl`, but is not listed above, please report it
by opening new issue at https://github.com/mvp/uhubctl/issues,
so we can add it to supported table. In your report, please provide
exact product model and add output from `uhubctl`.

Note that quite a few modern motherboards have built-in root hubs that
do support this feature - you may not even need to buy any external hub.
WARNING: turning off built-in USB ports may cut off your keyboard or mouse,
so be careful what ports you are turning off!


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
(Ubuntu/Debian, Redhat/Fedora/CentOS, Arch Linux, Gentoo, OpenSUSE, Buildroot), FreeBSD and Mac OS X.

While `uhubctl` compiles on Windows, USB power switching does not work on Windows because `libusb`
is using `winusb.sys` driver, which according to Microsoft does not support
[necessary USB control requests](https://social.msdn.microsoft.com/Forums/sqlserver/en-US/f680b63f-ca4f-4e52-baa9-9e64f8eee101).
This may be fixed if `libusb` starts supporting different driver on Windows.

First, you need to install library libusb-1.0 (version 1.0.12 or later):

* Ubuntu: `sudo apt-get install libusb-1.0-0-dev`
* Redhat: `sudo yum install libusb1-devel`
* MacOSX: `brew install libusb`, or `sudo port install libusb-devel`
* FreeBSD: libusb is included by default
* Windows: TBD?

To compile, simply run `make` - this will generate `uhubctl` binary.

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

On Linux, you may need to run it with `sudo`, or to configure `udev` USB permissions.

If you have more than one smart USB hub connected, you should choose
specific hub to control using `-l` (location) parameter.
To find hub locations, simply run `uhubctl` without any parameters.
Hub locations look like `b-x.y.z`, where `b` is USB bus number, and `x`, `y`, `z`...
are port numbers for all hubs in chain, starting from root hub for a given USB bus.
This address is semi-stable - it will not change if you unplug/replug (or turn off/on)
USB device into the same physical USB port (this method is also used in Linux kernel).


Copyright
=========

Copyright (C) 2009-2018 Vadim Mikhailov

This file can be distributed under the terms and conditions of the
GNU General Public License version 2.
