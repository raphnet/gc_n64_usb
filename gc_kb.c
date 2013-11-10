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

#define GC_KB_REPORT_SIZE	3

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
/*		0x19, 0xE0, // Usage Minimum (224)
		0x29, 0xE7, // Usage Maximum (231)
		0x15, 0x00, // Logical Minimum (0)
		0x25, 0x01, // Logical Maximum (1)
		
			// Modifier Byte
		0x75, 0x01, // Report Size(1)
		0x95, 0x08, // Report Count(8)
		0x81, 0x02, // Input (Data, Variable, Absolute)*/

			// Reserved Byte
//		0x95, 0x01, // Report Count(1)
//		0x75, 0x08, // Report Size(8)

		0x95, 0x03, // Report Count(3)
		0x75, 0x08, // Report Size(8)
		0x15, 0x00, // Logical Minimum (0)
		0x25, 0xE7, // Logical maximum (231)

			// Key array
//		0x05, 0x07, // Usage Page (key Codes)
		0x19, 0x00, // Usage Minimum(0)
		0x29, 0xE7, // Usage Maximum(231)
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

static const unsigned char gc_to_hid_table[] PROGMEM = {
	GC_KEY_RESERVED,		0x00,
	GC_KEY_HOME,			0x4A,
	GC_KEY_END,				0x4D,
	GC_KEY_PGUP,			0x4B,
	GC_KEY_PGDN,			0x4E,
	GC_KEY_SCROLL_LOCK,		0x47,
	GC_KEY_DASH_UNDERSCORE, 0x2D,
	GC_KEY_PLUS_EQUAL,		0x2E,
	GC_KEY_YEN,				0x89,
	GC_KEY_OPEN_BRKT_BRACE,	0x2F,
	GC_KEY_SEMI_COLON_COLON,0x33,
	GC_KEY_QUOTES,			0x34,
	GC_KEY_CLOSE_BRKT_BRACE,0x30,
	GC_KEY_BRACKET_MU,		0x32,
	GC_KEY_COMMA_ST,		0x36,
	GC_KEY_PERIOD_GT,		0x37,
	GC_KEY_SLASH_QUESTION,	0x38,
	GC_KEY_INTERNATIONAL1,	0x87,
	GC_KEY_ESC,				0x29,
	GC_KEY_INSERT,			0x49,
	GC_KEY_DELETE,			0x4c,
	GC_KEY_HANKAKU,			0x35,
	GC_KEY_BACKSPACE,		0x2A,
	GC_KEY_TAB,				0x2B,
	GC_KEY_CAPS_LOCK,		0x39,
	GC_KEY_LEFT_SHIFT,		0xE1,
	GC_KEY_RIGHT_SHIFT,		0xE5,
	GC_KEY_LEFT_CTRL,		0xE0,
	GC_KEY_LEFT_ALT,		0xE2,
	GC_KEY_MUHENKAN,		0x8B,
	GC_KEY_SPACE,			0x2C,
	GC_KEY_HENKAN,			0x8A,	
	GC_KEY_KANA,			0x88,
	GC_KEY_LEFT,			0x50,
	GC_KEY_DOWN,			0x51,
	GC_KEY_UP,				0x52,
	GC_KEY_RIGHT,			0x4F,
	GC_KEY_ENTER,			0x58,
};

unsigned char gcKeycodeToHID(unsigned char gc_code)
{
	int i;

	if (gc_code >= GC_KEY_A && gc_code <= GC_KEY_0) {
		return (gc_code - GC_KEY_A) + 0x04; // HID A
	}
	if (gc_code >= GC_KEY_F1 && gc_code <= GC_KEY_F12) {
		return (gc_code - GC_KEY_F1) + 0x3A; // HID F1
	}

	for (i=0; i<sizeof(gc_to_hid_table); i++) {
		if (pgm_read_byte(gc_to_hid_table + i*2) == gc_code) {
			return pgm_read_byte(gc_to_hid_table + i*2 + 1);
		}
	}

	return 0x38; // HID /? key for unknown keys
}

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

	last_built_report[0] = gcKeycodeToHID(tmpdata[4]);
	last_built_report[1] = gcKeycodeToHID(tmpdata[5]);
	last_built_report[2] = gcKeycodeToHID(tmpdata[6]);

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

static Gamepad GamecubeGamepad = {
	.num_reports			= 1,
	.init					= gamecubeInit,
	.update					= gamecubeUpdate,
	.changed				= gamecubeChanged,
	.buildReport			= gamecubeBuildReport,
	.probe					= gamecubeProbe,
};

Gamepad *gc_kb_getGamepad(void)
{
	GamecubeGamepad.reportDescriptor = (void*)gcKeyboardReport;
	GamecubeGamepad.reportDescriptorSize = sizeof(gcKeyboardReport);
	GamecubeGamepad.deviceDescriptor = (void*)gcKeyboardDevDesc;
	GamecubeGamepad.deviceDescriptorSize = sizeof(gcKeyboardDevDesc);
	return &GamecubeGamepad;
}

