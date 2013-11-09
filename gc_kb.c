/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
	Copyright (C) 2007-2013  Raphael Assenat <raph@raphnet.net>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "usbdrv/usbdrv.h"
#include "gamepad.h"
#include "leds.h"
#include "gc_kb.h"
#include "gcn64_protocol.h"

/*********** prototypes *************/
static void gamecubeInit(void);
static char gamecubeUpdate(void);
static char gamecubeChanged(int rid);

#define GC_KB_REPORT_SIZE	4

/* What was most recently read from the controller */
static unsigned char last_built_report[GC_KB_REPORT_SIZE];

/* What was most recently sent to the host */
static unsigned char last_sent_report[GC_KB_REPORT_SIZE];

/* [0] Modifier byte
 * [1] Key array
 * [2] Key array
 * [3] Key array
 *
 * See Universal Serial Bus HID Tables - 10 Keyboard/Keypad Page (0x07)
 * for key codes.
 *
 */
static const unsigned char gcKeyboardReport[] PROGMEM = {
	0x05, 0x01, // Usage page : Generic Desktop
	0x09, 0x06, // Usage (Keyboard)
	0xA1, 0x01, // Collection (Application)
		0x05, 0x07, // Usage Page (Key Codes)
		0x19, 0xE0, // Usage Minimum (224)
		0x29, 0xE7, // Usage Maximum (231)
		0x15, 0x00, // Logical Minimum (0)
		0x25, 0x01, // Logical Maximum (1)
		
			// Modifier Byte
		0x75, 0x01, // Report Size(1)
		0x95, 0x08, // Report Count(8)
		0x81, 0x02, // Input (Data, Variable, Absolute)

			// Reserved Byte
//		0x95, 0x01, // Report Count(1)
//		0x75, 0x08, // Report Size(8)

		0x95, 0x03, // Report Count(3)
		0x75, 0x08, // Report Size(8)
		0x15, 0x00, // Logical Minimum (0)
		0x25, 0x8B, // Logical maximum (139)

			// Key array
//		0x05, 0x07, // Usage Page (key Codes)
		0x19, 0x00, // Usage Minimum(0)
		0x29, 0x8B, // Usage Maximum(139)
		0x81, 0x00, // Input (Data, Array)

    0xc0,                          // END_COLLECTION
};

static const unsigned char gcKeyboardDevDesc[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
	0x9B, 0x28,	// Vendor ID
    0x0B, 0x00, // Product ID
	0x00, 0x01, // Version: Minor, Major
	1, // Manufacturer String
	2, // Product string
	3, // Serial number string
    1, /* number of configurations */
};



static int gc_rumbling = 0;
static int gc_analog_lr_disable = 0;

static void gamecubeInit(void)
{
	if (0 == gamecubeUpdate()) {
		if (gcn64_workbuf[GC_BTN_L] && gcn64_workbuf[GC_BTN_R]) {
			gc_analog_lr_disable = 1;
		} else {
			gc_analog_lr_disable = 0;
		}
	}
}

static char gamecubeUpdate(void)
{
	int i;
	unsigned char tmp=0;
	unsigned char tmpdata[8];	
	unsigned char count;

	tmpdata[0] = GC_POLL_KB1;
	tmpdata[1] = GC_POLL_KB2;
	tmpdata[2] = GC_POLL_KB3;

	count = gcn64_transaction(tmpdata, 3);
	if (count != 64) {
		return 1; // failure
	}

	gcn64_protocol_getBytes(0, 8, tmpdata);

	// report id
	last_built_report[0] = 0;
	last_built_report[1] = tmpdata[4];
	last_built_report[2] = tmpdata[5];
	last_built_report[3] = tmpdata[6];

	return 0; // success
}

static char gamecubeProbe(void)
{
	if (0 == gamecubeUpdate())
		return 1;

	return 0;
}

static char gamecubeChanged(int id)
{
	return memcmp(last_built_report, last_sent_report, GC_KB_REPORT_SIZE);
}

static int gamecubeBuildReport(unsigned char *reportBuffer, int id)
{
	if (reportBuffer != NULL)
		memcpy(reportBuffer, last_built_report, GC_KB_REPORT_SIZE);
	
	memcpy(last_sent_report, last_built_report, GC_KB_REPORT_SIZE);	
	return GC_KB_REPORT_SIZE;
}

static void gamecubeVibration(int value)
{
	gc_rumbling = value;
}

static Gamepad GamecubeGamepad = {
	.num_reports			= 1,
	.init					= gamecubeInit,
	.update					= gamecubeUpdate,
	.changed				= gamecubeChanged,
	.buildReport			= gamecubeBuildReport,
	.probe					= gamecubeProbe,
	.setVibration			= gamecubeVibration,
};

Gamepad *gc_kb_getGamepad(void)
{
	GamecubeGamepad.reportDescriptor = (void*)gcKeyboardReport;
	GamecubeGamepad.reportDescriptorSize = sizeof(gcKeyboardReport);
	GamecubeGamepad.deviceDescriptor = (void*)gcKeyboardDevDesc;
	GamecubeGamepad.deviceDescriptorSize = sizeof(gcKeyboardDevDesc);
	return &GamecubeGamepad;
}

