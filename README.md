uhubctl
=======

uhubctl is utility to control USB power per-port on smart USB hubs.
Smart hub is defined as one that implements per-port power switching.

Original idea for this code was inspired by hub-ctrl.c by Niibe Yutaka:
http://www.gniibe.org/development/ac-power-control-by-USB-hub


Compatible USB hubs
===================

Note that very few hubs actually support per-port power switching.
Some of them are no longer manufactured and can be hard to find.

This is list of known compatible USB hubs:

| Manufacturer       | Product                                                | VID:PID   | Release | EOL  |
|:-------------------|:-------------------------------------------------------|:----------|:--------|:-----|
| AmazonBasics       | HU3770V1, 7 Port USB 3.0 Hub with 12V/3A Power Adapter |`2109:2811`| 2013    |      |
| Apple              | Thunderbolt Display 27" (internal USB hub)             |           | 2011    | 2016 |
| Apple              | USB Keyboard With Numeric Pad (internal USB hub)       |           | 2011    |      |
| Asus               | Z87-PLUS Motherboard (onboard USB hubs)                |           | 2013    | 2016 |
| B&B Electronics    | UHR204                                                 |           | 2013    |      |
| Belkin             | F5U701-BLK                                             |           | 2008    | 2012 |
| Circuitco          | Beagleboard-xM (internal USB hub)                      |`0424:9514`| 2010    |      |
| CyberPower         | CP-H420P                                               |`0409:0059`| 2004    |      |
| D-Link             | DUB-H7 (silver edition only, new black not working)    |`2001:F103`| 2005    | 2010 |
| Elecom             | U2H-G4S                                                |           | 2006    | 2011 |
| Hawking Technology | UH214                                                  |           | 2003    | 2008 |
| Lenovo             | ThinkPad EU Ultra Dockingstation (40A20090EU)          |`17EF:100F`| 2015    |      |
| Lenovo             | ThinkPad X200 Ultrabase 42X4963                        |`17EF:1005`| 2008    | 2011 |
| Linksys            | USB2HUB4                                               |           | 2004    | 2010 |
| Maplin             | A08CQ                                                  |`0409:0059`| 2008    | 2011 |
| Microchip          | EVB-USB2517                                            |           | 2008    |      |
| Plugable           | USB2-HUB10S                                            |           | 2010    |      |
| Raspberry Pi       | Model B+, Model 2 B, Model 3 B                         |           | 2011    |      |
| Rosewill           | RHUB-210                                               |`0409:005A`| 2011    | 2014 |
| Sanwa Supply       | USB-HUB14GPH                                           |           | 2001    | 2003 |
| Sunix              | SHB4200MA                                              |`0409:0058`| 2006    | 2009 |
| Targus             | PAUH212U                                               |           | 2004    | 2009 |

This table is by no means complete.
If your hub works with uhubctl, but is not listed above, please report it
by opening new issue at https://github.com/mvp/uhubctl/issues,
so we can add it to supported table. In your report, please provide
exact product model and add output from uhubctl.

Note that quite a few modern motherboards have built-in root hubs that
do support this feature - you may not even need to buy any external hub.
WARNING: turning off built-in USB ports may cut off your keyboard or mouse,
so be careful what ports you are turning off!


Compiling
=========

This utility was tested to compile and work on Linux
(Ubuntu, Redhat/Fedora/CentOS), FreeBSD and Mac OS X.

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

Alternatively, for macOS you can build an executable with homebrew's custom tap:

```
brew tap mvp/uhubctl https://github.com/mvp/uhubctl/
brew install --HEAD uhubctl
```

Usage
=====

You can control the power on a USB port(s) like this:

    uhubctl -a off -p 235

This means operate on default smart hub and turn power off (`-a off`, or `-a 0`)
on ports 2,3,5 (`-p 235`). Supported actions are `off`/`on`/`cycle` (or `0`/`1`/`2`).
`cycle` means turn power off, wait some delay (configurable with `-d`) and turn it back on.

On Linux, you may need to run it with `sudo`, or to configure `udev` USB permissions.

If you have more than one smart USB hub connected, you should choose
specific hub to control using `-l` (location) parameter.


Copyright
=========

Copyright (C) 2009-2017 Vadim Mikhailov

This file can be distributed under the terms and conditions of the
GNU General Public License version 2.
