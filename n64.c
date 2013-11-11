/*	gc_n64_usb : Gamecube or N64 controller to USB firmware
	Copyright (C) 2007-2011  Raphael Assenat <raph@raphnet.net>

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
#include <util/delay.h>
#include <string.h>
#include "gamepad.h"
#include "leds.h"
#include "n64.h"
#include "reportdesc.h"
#include "gcn64_protocol.h"
#include "usbdrv.h"

/*********** prototypes *************/
static void n64Init(void);
static char n64Update(void);
static char n64Changed(int id);
static int n64BuildReport(unsigned char *reportBuffer, int id);


/* What was most recently read from the controller */
static unsigned char last_built_report[GCN64_REPORT_SIZE];

/* What was most recently sent to the host */
static unsigned char last_sent_report[GCN64_REPORT_SIZE];

static void n64Init(void)
{
	// rumble on debug
	DDRC |= 0x01; // PC0
	PORTC &= ~0x01;
	n64Update();
}

#define RSTATE_INIT			0
#define RSTATE_OFF			1
#define RSTATE_TURNON		2
#define RSTATE_ON			3
#define RSTATE_TURNOFF		4
#define RSTATE_UNAVAILABLE	5
static unsigned char n64_rumble_state = RSTATE_UNAVAILABLE;

static char n64Update(void)
{
	int i;
	unsigned char tmpdata[38];
	unsigned char count;
	unsigned char x,y;
	unsigned char btns1, btns2;
	unsigned char rb1, rb2;
	unsigned char caps[3];

	/* Pad answer to N64_GET_CAPABILITIES
	 *
	 * 0x050000 : 0000 0101 0000 0000 0000 0000 : No expansion pack
	 * 0x050001 : 0000 0101 0000 0000 0000 0001 : With expansion pack
	 * 0x050002 : 0000 0101 0000 0000 0000 0010 : Expansion pack removed
	 *
	 * Bit 0 tells us if there is something connected to the expansion port.
	 * Bit 1 tells is if there was something connected that has been removed.
	 */
	tmpdata[0] = N64_GET_CAPABILITIES;
	count = gcn64_transaction(tmpdata, 1);
	if (count != N64_CAPS_REPLY_LENGTH) {
		// a failed read could mean the pack or controller was gone. Init
		// will be necessary next time we detect a pack is present.
		n64_rumble_state = RSTATE_INIT;
		return -1;
	}

	caps[0] = gcn64_protocol_getByte(0);
	caps[1] = gcn64_protocol_getByte(1);
	caps[2] = gcn64_protocol_getByte(2);
	
	/* Detect when a pack becomes present and schedule initialisation when it happens. */
	//if (gcn64_workbuf[OFFSET_EXT_PRESENT] && (n64_rumble_state == RSTATE_UNAVAILABLE)) {
	if ((caps[2] & 0x01) && (n64_rumble_state == RSTATE_UNAVAILABLE)) {
		n64_rumble_state = RSTATE_INIT;	
	}

	/* Detect when a pack is removed. */
	if (!(caps[2] & 0x01)) {
		n64_rumble_state = RSTATE_UNAVAILABLE;
	}

	switch (n64_rumble_state)
	{
		case RSTATE_INIT:
			tmpdata[0] = N64_EXPANSION_WRITE;
			tmpdata[1] = 0x80;
			tmpdata[2] = 0x01;
			memset(tmpdata+3, 0x80, 34);
			count = gcn64_transaction(tmpdata, 35);
			if (count > 0) {
				/* Answer: 1011 1000 (0xb8) */
				n64_rumble_state = RSTATE_TURNOFF;
			}
			break;

		case RSTATE_TURNON:
			tmpdata[0] = N64_EXPANSION_WRITE;
			tmpdata[1] = 0xc0;
			tmpdata[2] = 0x1b;
			memset(tmpdata+3, 0x01, 32);
			count = gcn64_transaction(tmpdata, 35);
			if (count > 0) {
				n64_rumble_state = RSTATE_ON;
			}
			break;

		case RSTATE_TURNOFF:
			tmpdata[0] = N64_EXPANSION_WRITE;
			tmpdata[1] = 0xc0;
			tmpdata[2] = 0x1b;
			memset(tmpdata+3, 0x00, 32);
			count = gcn64_transaction(tmpdata, 35);
			if (count > 0) {
				n64_rumble_state = RSTATE_OFF;
			}
			break;
	}

	tmpdata[0] = N64_GET_STATUS;
	count = gcn64_transaction(tmpdata, 1);
	if (count != N64_GET_STATUS_REPLY_LENGTH) {
		return -1;
	}

/*
	Bit	Function
	0	A
	1	B
	2	Z
	3	Start
	4	Directional Up
	5	Directional Down
	6	Directional Left
	7	Directional Right
	8	unknown (always 0)
	9	unknown (always 0)
	10	L
	11	R
	12	C Up
	13	C Down
	14	C Left
	15	C Right
	16-23: analog X axis
	24-31: analog Y axis
 */

	btns1 = gcn64_protocol_getByte(0);
	btns2 = gcn64_protocol_getByte(8);
	x = gcn64_protocol_getByte(16); // X axis
	y = gcn64_protocol_getByte(24); // Y axis

	rb1 = rb2 = 0;

	// Remap buttons as they always were by this 
	// adapter. Might change in v3 when a N64
	// specific report descriptor will be used.
	//
	for (i=0; i<4; i++) // A B Z START
		rb1 |= (btns1 & (0x80 >> i)) ? (0x01<<i) : 0;
	for (i=0; i<4; i++) // C-UP C-DOWN C-LEFT C-RIGHT
		rb1 |= btns2 & (0x08 >> i) ? (0x10<<i) : 0;
	for (i=0; i<2; i++) // L R
		rb2 |= btns2 & (0x20 >> i) ? (0x01<<i) : 0;
	for (i=0; i<4; i++) // Up down left right
		rb2 |= btns1 & (0x08 >> i) ? (0x04<<i) : 0;
	
	// analog joystick
	last_built_report[0] = 1;
	last_built_report[1] = ((int)((signed char)x))+127;
	last_built_report[2] = ((int)( -((signed char)y)) )+127;

	last_built_report[3] = 0x7f;
	last_built_report[4] = 0x7f;
	last_built_report[5] = 0x7f;
	last_built_report[6] = 0x7f;

	// buttons
	last_built_report[7] = rb1;
	last_built_report[8] = rb2;

	return 0;
}

static char n64Probe(void)
{
	int count;
	char i;
	unsigned char tmp;

	/* Pad answer to N64_GET_CAPABILITIES
	 *
	 * 0x050000 : 0000 0101 0000 0000 0000 0000 : No expansion pack
	 * 0x050001 : 0000 0101 0000 0000 0000 0001 : With expansion pack
	 * 0x050002 : 0000 0101 0000 0000 0000 0010 : Expansion pack removed
	 *
	 * Bit 0 tells us if there is something connected to the expansion port.
	 * Bit 1 tells is if there was something connected that has been removed.
	 */

	n64_rumble_state = RSTATE_UNAVAILABLE;

	for (i=0; i<15; i++)
	{
		usbPoll(); // must be called at each 50ms or less
		_delay_ms(30);
		
		tmp = N64_GET_CAPABILITIES;
		count = gcn64_transaction(&tmp, 1);

		if (count == N64_CAPS_REPLY_LENGTH) {
			return 1;
		}
	}
	return 0;
}


static char n64Changed(int id)
{
	return memcmp(last_built_report, last_sent_report, GCN64_REPORT_SIZE);
}

static int n64BuildReport(unsigned char *reportBuffer, int id)
{
	if (reportBuffer) 
		memcpy(reportBuffer, last_built_report, GCN64_REPORT_SIZE);
	
	memcpy(	last_sent_report, last_built_report, GCN64_REPORT_SIZE);	
	return GCN64_REPORT_SIZE;
}

static void n64SetVibration(int value)
{
	if (n64_rumble_state == RSTATE_UNAVAILABLE || 
			n64_rumble_state == RSTATE_INIT) 
	{
		// Sorry, not ready or not possible (no pack)
		return;
	}

	if (value) {
		if (n64_rumble_state != RSTATE_ON) {
			n64_rumble_state = RSTATE_TURNON;
		}
	} else {
		if (n64_rumble_state != RSTATE_OFF) {
			n64_rumble_state = RSTATE_TURNOFF;
		}
	}
}

static Gamepad N64Gamepad = {
	.init					= n64Init,
	.update					= n64Update,
	.changed				= n64Changed,
	.buildReport			= n64BuildReport,
	.probe					= n64Probe,
	.num_reports			= 1,
	.setVibration			= n64SetVibration,
};

Gamepad *n64GetGamepad(void)
{
	N64Gamepad.reportDescriptor = (void*)gcn64_usbHidReportDescriptor;
	N64Gamepad.reportDescriptorSize = getUsbHidReportDescriptor_size();
	return &N64Gamepad;
}

