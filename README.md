# N64/Gamecube controller to USB adapter (V-USB based)

## What is (or was) gc_n64_usb?

gc_n64_usb if a firmware for Atmel AVR micro-controllers which
allows one to connect an Nintendo 64 or Gamecube controller
to a PC with an USB port. The joystick is implemented
as a standard HID device so no special drivers are required.

Note: There is a new version of this project for AVR micro-controllers
with native USB support (eg: Atmega32u2). I am now focusing mainly on this
new version, which is much better already:

* English: [Gamecube/N64 controller to USB adapter (Third generation)](http://www.raphnet.net/electronique/gcn64_usb_adapter_gen3/index_en.php)
* French: [Adaptateur manette Gamecube/N64 à USB (Troisième génération)](http://www.raphnet.net/electronique/gcn64_usb_adapter_gen3/index.php)


## Project homepage

Schematics and additional information are available on the project homepage:

* English: [N64/Gamecube controller to USB adapter](http://www.raphnet.net/electronique/gc_n64_usb/index_en.php)
* French: [Convertisseur manette N64/Gamecube à USB](http://www.raphnet.net/electronique/gc_n64_usb/index.php)


## What about the new version mentioned above?

gc_n64_usb uses V-USB, a software-only usb driver from Objective Development.
See [V-USB](http://www.obdev.at/products/avrusb/index.html)

At the time this project was started in 2007, I used this software-only approach
since it was cheap, and easy to solder chips (DIP packages) were available. But
there are limitations to this approach:

* Many CPU cycles are wasted for USB communication
* Interrupts may not be disabled. Timing-sensitive N64/Gamceube communication may be interrupted by USB activity at any time, leading to rx/tx failures, retries, and in extreme cases controller resets (and in that case unwanted joystick re-calibration). The code works around this by synchronizing GC/N64 polling with USB.
* USB is limited to low speed, which implies that
  * Transfer size is limited to chunks of 8 bytes. Reports of 8 bytes or more must therefore be sent in two parts.
  * The endpoint bInterval value is supposed to be no lower than 10ms, increasing latency. The project cheats and set it to 5ms anyway.. (Which results in approximate 10ms USB-contributed latency, since reports are 9 bytes...

The above is what led to the development of the new version mentionned in the introduction.


## Features

* Supports Wired Gamecube controllers and dance mats (Official and clones)
* Supports Wireless controllers (Known to work at least with the Nintendo Wavebird (since firmware version 1.2) and an Intec wireless controller).
* Supports N64 Controllers (Official and clones, including the famous HORI-mini)
* Supports the N64 "Rumble Pack" and the Gamecube controller built-in vibration function. (Since release 2.0)
* Supports the Gamecube Keyboard (ASCII ASC-1901P0 tested) since release 2.9
* Supports the DK Bongos.


## Supported micro-controllers

Currently supported micro-controller(s):

* Atmega8 (Makefile.atmega8)
* Atmega88 (Makefile.atmega88 and Makefile.atmega88p)
* Atmega168 (Makefile.atmega168)

Adding support for other micro-controllers should be easy, as long as the target has enough
IO pins, enough memory (flash and SRAM) and is supported by V-USB.

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

Please do not re-use them for other projects. Instead, please use your own.

VID/PID conflicts, especially with some Windows version caching of HID descriptors,
can lead to complicated and hard to debug problems...


## Acknowledgments

* Thank you to Objective development, author of [V-USB](https://www.obdev.at/products/vusb/index.html) for a wonderful software-only USB device implementation.

A good portion of gc_n64_usb is based on Objective Development's HIDKeys example.

