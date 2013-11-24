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
		unsigned char btns2;

		btns2 = gcn64_protocol_getByte(8);

		//if (gcn64_workbuf[GC_BTN_L] && gcn64_workbuf[GC_BTN_R]) {
		if ((btns2 & 0x06) == 0x06) { // L + R
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
	unsigned char x,y,cx,cy,rtrig,ltrig,btns1,btns2,rb1,rb2;
	
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
	
	btns1 = gcn64_protocol_getByte(0);
	btns2 = gcn64_protocol_getByte(8);
	x = gcn64_protocol_getByte(16);
	y = gcn64_protocol_getByte(24);
	cx = gcn64_protocol_getByte(32);
	cy = gcn64_protocol_getByte(40);
	ltrig = gcn64_protocol_getByte(48);
	rtrig = gcn64_protocol_getByte(56);

	/* Prepare button bits */
	rb1 = rb2 = 0;
	for (i=0; i<5; i++) // St Y X B A
		rb1 |= (btns1 & (0x10 >> i)) ? (0x01<<i) : 0;
	for (i=0; i<3; i++) // L R Z
		rb1 |= (btns2 & (0x40 >> i)) ? (0x20<<i) : 0;
	for (i=0; i<4; i++) // Up,Down,Right,Left
		rb2 |= (btns2 & (0x08 >> i)) ? (0x01<<i) : 0;

	if (gc_analog_lr_disable) {
		ltrig = 0x7f;
		rtrig = 0x7f;
	}

	last_built_report[0] = 1; // report ID
	last_built_report[1] = x;
	last_built_report[2] = y ^ 0xff;
	last_built_report[3] = cx;
	last_built_report[4] = cy ^ 0xff;
	// Sliders value to decrease as pushed (v2.x behaviour)
	last_built_report[5] = ltrig ^ 0xff;
	last_built_report[6] = rtrig ^ 0xff;
	last_built_report[7] = rb1;
	last_built_report[8] = rb2;

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

Gamepad *gamecubeGetGamepad(void)
{
	GamecubeGamepad.reportDescriptor = (void*)gcn64_usbHidReportDescriptor;
	GamecubeGamepad.reportDescriptorSize = getUsbHidReportDescriptor_size();
	return &GamecubeGamepad;
}

