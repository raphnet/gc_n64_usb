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
#include "gamepad.h"
#include "leds.h"
#include "gc_kb.h"
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

	tmpdata[0] = GC_POLL_KB1;
	tmpdata[1] = GC_POLL_KB2;
	tmpdata[2] = GC_POLL_KB3;

	count = gcn64_transaction(tmpdata, 3);
	if (count != 8) {
//		return 1; // failure
	}

	gcn64_protocol_getBytes(0, 8, tmpdata);

	// report id
	last_built_report[0] = 1;

	last_built_report[1] = 0x7f; // X
	last_built_report[2] = 0x7f; // Y
	last_built_report[3] = 0x7f;
	last_built_report[4] = 0x7f;
	last_built_report[5] = 0x7f;
	last_built_report[6] = 0x7f;
	last_built_report[7] = tmpdata[4];
	last_built_report[8] = tmpdata[5];

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

Gamepad *gc_kb_getGamepad(void)
{
	GamecubeGamepad.reportDescriptor = (void*)gcn64_usbHidReportDescriptor;
	GamecubeGamepad.reportDescriptorSize = getUsbHidReportDescriptor_size();
	return &GamecubeGamepad;
}

