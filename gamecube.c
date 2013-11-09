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
#include <avr/pgmspace.h>
#include <string.h>
#include "gamepad.h"
#include "leds.h"
#include "gamecube.h"
#include "reportdesc.h"
#include "gcn64_protocol.h"

/*********** prototypes *************/
static void gamecubeInit(void);
static char gamecubeUpdate(void);
static char gamecubeChanged(int rid);


/* What was most recently read from the controller */
static unsigned char last_built_report[GCN64_REPORT_SIZE];

/* What was most recently sent to the host */
static unsigned char last_sent_report[GCN64_REPORT_SIZE];

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
	
	/* Get ID command.
	 * 
	 * If we don't do that, the wavebird does not work.  
	 */
	tmp = GC_GETID;
	count = gcn64_transaction(&tmp, 1);
	if (count != GC_GETID_REPLY_LENGTH) {
		return 1;
	}

	tmpdata[0] = GC_GETSTATUS1;
	tmpdata[1] = GC_GETSTATUS2;
	tmpdata[2] = GC_GETSTATUS3(gc_rumbling);

	count = gcn64_transaction(tmpdata, 3);
	if (count != GC_GETSTATUS_REPLY_LENGTH) {
		return 1; // failure
	}
	
/*
	(Source: Nintendo Gamecube Controller Protocol
		updated 8th March 2004, by James.)

	Bit		Function
	0-2		Always 0 
	3		Start
	4		Y
	5		X
	6		B
	7		A
	8		Always 1
	9		L
	10		R
	11		Z
	12-15	Up,Down,Right,Left
	16-23	Joy X
	24-31	Joy Y
	32-39	C Joystick X
	40-47	C Joystick Y
	48-55	Left Btn Val
	56-63	Right Btn Val
 */
	
	/* Convert the one-byte-per-bit data generated
	 * by the assembler mess above to nicely packed
	 * binary data. */	
	memset(tmpdata, 0, sizeof(tmpdata));

	for (i=0; i<8; i++) // X axis
		tmpdata[0] |= gcn64_workbuf[i+16] ? (0x80>>i) : 0;
	
	for (i=0; i<8; i++) // Y axis
		tmpdata[1] |= gcn64_workbuf[i+24] ? (0x80>>i) : 0;
	tmpdata[1] ^= 0xff;

	for (i=0; i<8; i++) // C X axis
		tmpdata[2] |= gcn64_workbuf[i+32] ? (0x80>>i) : 0;

	// Raph: October 2007. C stick vertical orientation 
	// was wrong. But no one complained...
#if INVERTED_VERTICAL_C_STICK
	for (i=0; i<8; i++) // C Y axis
		tmpdata[3] |= gcn64_workbuf[i+40] ? (0x80>>i) : 0;	
#else
	for (i=0; i<8; i++) // C Y axis
		tmpdata[3] |= gcn64_workbuf[i+40] ? 0 : (0x80>>i);	
#endif

	for (i=0; i<8; i++) // Left btn value
		tmpdata[4] |= gcn64_workbuf[i+48] ? (0x80>>i) : 0;
	tmpdata[4] ^= 0xff;
	
	for (i=0; i<8; i++) // Right btn value
		tmpdata[5] |= gcn64_workbuf[i+56] ? (0x80>>i) : 0;	
	tmpdata[5] ^= 0xff;

	for (i=0; i<5; i++) // St Y X B A
		tmpdata[6] |= gcn64_workbuf[i+3] ? (0x01<<i) : 0;
	for (i=0; i<3; i++) // L R Z
		tmpdata[6] |= gcn64_workbuf[i+9] ? (0x20<<i) : 0;
	
	for (i=0; i<4; i++) // Up,Down,Right,Left
		tmpdata[7] |= gcn64_workbuf[i+12] ? (0x01<<i) : 0;

	// report id
	last_built_report[0] = 1;

	memcpy(last_built_report + 1, tmpdata, 6);

	if (gc_analog_lr_disable) {
		last_built_report[5] = 0x7f;
		last_built_report[6] = 0x7f;
	}
	last_built_report[7] = tmpdata[6];
	last_built_report[8] = tmpdata[7];

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
	return memcmp(last_built_report, last_sent_report, GCN64_REPORT_SIZE);
}

static int gamecubeBuildReport(unsigned char *reportBuffer, int id)
{
	if (reportBuffer != NULL)
		memcpy(reportBuffer, last_built_report, GCN64_REPORT_SIZE);
	
	memcpy(last_sent_report, last_built_report, GCN64_REPORT_SIZE);	
	return GCN64_REPORT_SIZE;
}

void gamecubeVibration(int value)
{
	gc_rumbling = value;
}

Gamepad GamecubeGamepad = {
	.num_reports			= 1,
	.init					= gamecubeInit,
	.update					= gamecubeUpdate,
	.changed				= gamecubeChanged,
	.buildReport			= gamecubeBuildReport,
	.probe					= gamecubeProbe,
	.setVibration			= gamecubeVibration,
};

Gamepad *gamecubeGetGamepad(void)
{
	GamecubeGamepad.reportDescriptor = (void*)gcn64_usbHidReportDescriptor;
	GamecubeGamepad.reportDescriptorSize = getUsbHidReportDescriptor_size();
	return &GamecubeGamepad;
}

