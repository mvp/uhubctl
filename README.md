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

* D-Link: DUB-H7 (Silver edition, new Black edition is not compatible!)
* Elecom: U2H-G4S
* Sanwa Supply: USB-HUB14GPH
* Targus: PAUH212
* Hawking Technology: UH214
* Apple: USB Keyboard With Numeric Pad (internal USB hub)
* Apple: Thunderbolt Display 27" (internal USB hub)
* Linksys: USB2HUB4

Some modern motherboards have built-in root hubs that do support
this feature, but you need to use option `-i` to enable it.


Compiling
=========

This utility was tested to compile and work on Linux
(Ubuntu, Redhat/Fedora/CentOS) and on Mac OS X.
It should be possible to compile it for Windows as well -
please report if you succeed in doing that.

First, you need to install library libusb-1.0 (version 1.0.12 or later):

* Ubuntu: sudo apt-get install libusb-1.0-0-dev
* Redhat: sudo yum install libusb1-devel
* MacOSX: brew install libusb
* Windows: TBD?

To compile, simply run `make` - this will generate `uhubctl` binary.

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

Copyright (C) 2009-2016 Vadim Mikhailov

This file can be distributed under the terms and conditions of the
GNU General Public License version 2.
