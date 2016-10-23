# N64/Gamecube controller to USB adapter

## What is gc_n64_usb?

gc_n64_usb if a firmware for Atmel AVR micro-controllers which
allows one to connect an Nintendo 64 or Gamecube controller
to a PC with an USB port. The joystick is implemented
as a standard HID device so no special drivers are required.


## Project homepage

Schematics and additional information are available on the project homepage:

* English: [N64/Gamecube controller to USB adapter](http://www.raphnet.net/electronique/gc_n64_usb/index_en.php)
* French: [Convertisseur manette N64/Gamecube à USB](http://www.raphnet.net/electronique/gc_n64_usb/index.php)


## Features

* Supports Wired Gamecube controllers and dance mats (Official and clones)
* Supports Wireless controllers (Known to work at least with the Nintendo Wavebird (since firmware version 1.2) and an Intec wireless controller).
* Supports N64 Controllers (Official and clones, including the famous HORI-mini)
* Supports the N64 "Rumble Pack" and the Gamecube controller built-in vibration function. (Since release 2.0)
* Supports the Gamecube Keyboard (ASCII ASC-1901P0 tested) since release 2.9
* Supports the DK Bongos.


## Compilation and installation

To compile, you need a working avr-gcc and avr-libc. Under linux, simply type make
in the project directory. (assuming avr-gcc is in your path).

Next, you must upload the generated file (gc_n64_usb.hex) to the Atmega using
whatever tools you like. Personally, I use avrdude. The 'flash' and 'fuse'
targets in the makefile is a good example about how to use it.

The Atmega fuse bits must also be set properly. The idea behind this is to
enable the external 12mhz crystal instead of the internal clock. Check the
makefile for good fuse bytes values.


## License

Since version 1.3, gc_n64_usb is released under the terms of the GPLv2 license.

Older version were released under Objective Development's extended GPL license.


## About the vendor id/product id pair:

Please dont re-use them for other projects. Instead, please use your own.

VID/PID conflicts, especially with some Windows version caching of HID descriptors,
can lead to complicated and hard to debug problems...


## Acknowledgments

gc_n64_usb uses V-USB, a software-only usb driver from Objective Development.
See [V-USB](http://www.obdev.at/products/avrusb/index.html)

A good portion of gc_n64_usb is based on Objective Development's
HIDKeys example.
