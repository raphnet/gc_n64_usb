This is gc_n64_usb Readme.txt file. 

Table of contents:

1) What is gc_n64_usb?
2) USB Implementation
3) Compilation and installation
4) License
5) About the vendor id/product id pair:
6) Where do I get more information and updates?


1) What is gc_n64_usb?
   --------------------
	gc_n64_usb if a firmware for Atmel ATmega8 which 
	allows one to connect an Nintendo 64 or Gamecube controller
	to a PC with an USB port. The joystick is implemented
	as a standard HID device so no special drivers are required.

	At startup, the firmware looks for a controller. As long as
	there is no controller, the USB device will not enumerate. The 
	type of controller is auto-detected. If a Gamecube controller 
	and a N64 controller are connected at the same time, only 
	one will work.


2) USB Implementation
   ------------------
	gc_n64_usb uses the software-only usb driver from Objective Development.
	See http://www.obdev.at/products/avrusb/index.html

	A good portion of gc_n64_usb is based on Objective Development's
	HIDKeys example.


3) Compilation and installation
   ----------------------------
	First, you must compile it. To compile, you need a working avr-gcc and
	avr-libc. Under linux or cygwin, simply type make in the project directory.
	(assuming avr-gcc is in your path). 

	Next, you must upload the generated file (gc_n64_usb.hex) to the Atmega8 using
	whatever tools you like. Personally, I use uisp. The 'flash' and 'fuse'
	targets in the makefile is a good example about how to use it. 

	The Atmega fuse bits must also be set properly. The idea behind this is to
	enable the external 12mhz crystal instead of the internal clock. Check the
	makefile for good fuse bytes values.


4) License
   -------
	gc_n64_usb is released under Objective Development's extended GPL
	license. See License.txt


5) About the vendor id/product id pair:
   ------------------------------------
	Please dont re-use them for other projects. Instead, get your own. 
	I got mine from mecanique:
	http://www.mecanique.co.uk/products/usb/pid.html


6) Where do I get more information and updates?
   --------------------------------------------
	Visit my gc_n64_usb page:
	http://www.raphnet.net/electronique/gc_n64_usb/index_en.php
	you may also contact me by email:
	Raphaël Assénat <raph@raphnet.net>
	

