/* N64/Gamecube controller to USB adapter
 * Copyright (C) 2007-2013 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <string.h>

#include "usbdrv.h"
#include "oddebug.h"
#include "gamepad.h"
#include "gamecube.h"
#include "n64.h"
#include "gcn64_protocol.h"

#include "devdesc.h"
#include "reportdesc.h"

#define MAX_REPORTS	2

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega168A__) || \
	defined(__AVR_ATmega168P__) || defined(__AVR_ATmega328__) || \
	defined(__AVR_ATmega328P__) || defined(__AVR_ATmega88__) || \
	defined(__AVR_ATmega88A__) || defined(__AVR_ATmega88P__) || \
	defined(__AVR_ATmega88PA__)
#define AT168_COMPATIBLE
#endif

static uchar *rt_usbHidReportDescriptor=NULL;
static int rt_usbHidReportDescriptorSize=0;
static uchar *rt_usbDeviceDescriptor=NULL;
static uchar rt_usbDeviceDescriptorSize=0;

PROGMEM int usbDescriptorStringSerialNumber[] = {
	USB_STRING_DESCRIPTOR_HEADER(USB_CFG_SERIAL_NUMBER_LENGTH),
	'0', '0', '0', '1'
};

static Gamepad *curGamepad = NULL;


/* ----------------------- hardware I/O abstraction ------------------------ */

static void hardwareInit(void)
{
	/* PORTB
	 *
	 * Bit    Description       Direction    Level/PU     Note
	 * 0      Jumpers common    Out          0            Not used by this project
	 * 1      JP1               In           1            Not used by this project
	 * 2      JP2               In           1            Not used by this project
	 * 3      MOSI              In           1
	 * 4      MISO              In           1
	 * 5      SCK               In           1
	 * 6      -
	 * 7      -
	 */
	DDRB = 0x01;
	PORTB = 0xFE;

	// init port C as input with pullup.
	DDRC = 0x00;
	PORTC = 0xff;

	/*
	 * For port D, activate pull-ups on all lines except the D+, D- and bit 1.
	 *
	 * For historical reasons (a mistake on an old PCB), bit 1
	 * is now always connected with bit 0. So bit 1 configured
	 * as an input without pullup.
	 *
	 * Usb pin are init as output, low. (device reset). They are released
	 * later when usbReset() is called. 
	 */
	PORTD = 0xf8;
	DDRD = 0x01 | 0x04;
	
	/* Configure timers */
#if defined(AT168_COMPATIBLE)
	TCCR0A = 0; // normal
	TCCR0B = 5;
	TCCR2A= (1<<WGM21);
	TCCR2B=(1<<CS22)|(1<<CS21)|(1<<CS20);
	OCR2A= 50;  // for 240 hz
#else
	TCCR0 = 5; // divide by 1024 
	TCCR2 = (1<<WGM21)|(1<<CS22)|(1<<CS21)|(1<<CS20);
	OCR2 = 50; // for 240 hz
#endif

}

static void usbReset(void)
{
	/* [...] a single ended zero or SE0 can be used to signify a device
	 * reset if held for more than 10mS. A SE0 is generated by holding 
	 * both th D- and D+ low (< 0.3V). 
	 *
	 * Source: http://www.beyondlogic.org/usbnutshell/usb2.shtml
	 */
	PORTD &= ~(0x01 | 0x04); // Set D+ and D- to 0
	DDRD |= 0x01 | 0x04;
	_delay_ms(15);
	DDRD &= ~(0x01 | 0x04);
}

#if defined(AT168_COMPATIBLE)
	#define mustPollControllers()   (TIFR2 & (1<<OCF2A))
	#define clrPollControllers()    do { TIFR2 = 1<<OCF2A; } while(0)
	#define mustRunEffectLoop()		(TIFR0 & (1<<TOV0))
	#define clrRunEffectLoop()		do { TIFR0 = 1<<TOV0; } while(0)
#else
	#define mustPollControllers()   (TIFR & (1<<OCF2))
	#define clrPollControllers()    do { TIFR = 1<<OCF2; } while(0)
	#define mustRunEffectLoop()		(TIFR & (1<<TOV0))
	#define clrRunEffectLoop()		do { TIFR = 1<<TOV0; } while(0)
#endif

static uchar    reportBuffer[10];    /* buffer for HID reports */



/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */


usbMsgLen_t   usbFunctionDescriptor(struct usbRequest *rq)
{
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_STANDARD)
		return 0;

	if (rq->bRequest == USBRQ_GET_DESCRIPTOR)
	{
		// USB spec 9.4.3, high byte is descriptor type
		switch (rq->wValue.bytes[1])
		{
			case USBDESCR_DEVICE:
				usbMsgPtr = rt_usbDeviceDescriptor;
				return rt_usbDeviceDescriptorSize;

			case USBDESCR_HID_REPORT:
				usbMsgPtr = rt_usbHidReportDescriptor;
				return rt_usbHidReportDescriptorSize;

		}
	}

	return 0;
}

static void gamepadVibrate(char on)
{
	if (curGamepad)
		if (curGamepad->setVibration)
				curGamepad->setVibration(on);
}

static int getGamepadReport(unsigned char *dstbuf, int id)
{
	if (curGamepad == NULL) {
		if (id==1) {
			dstbuf[0] = 1;
			dstbuf[1] = 0x7f;
			dstbuf[2] = 0x7f;
			dstbuf[3] = 0x7f;
			dstbuf[4] = 0x7f;
			dstbuf[5] = 0x7f;
			dstbuf[6] = 0x7f;
			dstbuf[7] = 0;
			dstbuf[8] = 0;

			return 9;
		}
		return 0;
	}
	else {
		return curGamepad->buildReport(dstbuf, id);
	}
}

static unsigned char _FFB_effect_index;
#define LOOP_MAX	0xFFFF
static unsigned int _loop_count;

static void effect_loop()
{
	if (_loop_count) {
		if (_loop_count != LOOP_MAX) {
			_loop_count--;
		}
	}
}

// Output Report IDs for various functions
#define REPORT_SET_EFFECT			0x01
#define REPORT_SET_PERIODIC			0x04
#define REPORT_SET_CONSTANT_FORCE	0x05	
#define REPORT_EFFECT_OPERATION		0x0A
#define REPORT_CREATE_EFFECT		0x09
#define REPORT_PID_POOL				0x0D

// For the 'Usage Effect Operation' report
#define EFFECT_OP_START			1
#define EFFECT_OP_START_SOLO	2
#define EFFECT_OP_STOP			3

// Feature report
#define PID_SIMULTANEOUS_MAX	3
#define PID_BLOCK_LOAD_REPORT	2

usbMsgLen_t	usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (void *)data;

	usbMsgPtr = reportBuffer;
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
			
		switch(rq->bRequest)
		{
			case USBRQ_HID_GET_REPORT:
				{
					char error = 1;

					// USB 7.2.1 GET_report_request
					//
					// wValue high byte : report type
					// wValue low byte : report id
#if 1
					switch (rq->wValue.bytes[1]) // type
					{
						case 1: // input report
							if (rq->wValue.bytes[0]) {
								return getGamepadReport(reportBuffer, rq->wValue.bytes[0]);
							}
							if (rq->wValue.bytes[0] == 0x03) {
								reportBuffer[0] = rq->wValue.bytes[0];
								reportBuffer[1] = 1; // _FFB_effect_index;
								// 1 : Block load success, 2 : Block load full (OOM), 3 : block load error
								reportBuffer[2] = error;
								reportBuffer[3] = 1;
								reportBuffer[4] = 1;
	//							gamepadVibrate(1);
								return 5;
							}
													
							break;

						case 2: // output report
							break;

						case 3: // feature report
							if (rq->wValue.bytes[0] == PID_BLOCK_LOAD_REPORT) {
//								gamepadVibrate(1);
								reportBuffer[0] = rq->wValue.bytes[0];
								reportBuffer[1] = 0x1;
								reportBuffer[2] = 0x1;
								reportBuffer[3] = 10;
								reportBuffer[4] = 10;
								return 5;
							}
							else if (rq->wValue.bytes[0] == PID_SIMULTANEOUS_MAX) {
								reportBuffer[0] = rq->wValue.bytes[0];
								reportBuffer[1] = 1;
								reportBuffer[2] = 1;
								reportBuffer[3] = 0xff;
								reportBuffer[4] = 1;
								return 5;
							}
							break;
					}
#endif
				}

			case USBRQ_HID_SET_REPORT:
				{
					return USB_NO_MSG;	
				}
		}
	}else{
	/* no vendor specific requests implemented */
	}
	return 0;
}


static unsigned char vibration_on = 0;
static unsigned char constant_force = 0;
static unsigned char magnitude = 0;

static void decideVibration(void)
{
	if (!_loop_count)
		vibration_on = 0;

	if (!vibration_on)
	{
		gamepadVibrate(0);
	}
	else {
		if (constant_force > 0x7f) {
			gamepadVibrate(1);
		}
		if (magnitude > 0x7f) { 
			gamepadVibrate(1);
		}
	}
	
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	if (len < 1)
		return 1;

	//

	switch(data[0]) // Report ID
	{
		case REPORT_CREATE_EFFECT:
			_FFB_effect_index = data[1];
			break;

		case REPORT_SET_EFFECT:	
			/* Byte 0 : Effect block index
			 * Byte 1 : Effect type
			 * Bytes 2-3 : Duration
			 * Bytes 4-5 : Trigger repeat interval
			 * Bytes 6-7 : Sample period
			 * Byte 8 : Gain
			 * Byte 9 : Trigger button
			 */

		//	gamepadVibrate(1);
			_FFB_effect_index = data[1];
			break;

		case REPORT_SET_CONSTANT_FORCE:
			if (data[1]==1) {
				constant_force = data[2];
				decideVibration();
			}
			break;

		case REPORT_SET_PERIODIC:
			magnitude = data[2];
			decideVibration();
			break;

		case REPORT_EFFECT_OPERATION:
			if (len != 4)
				return 1;

			/* Byte 0 : report ID 
			 * Byte 1 : bit 7=rom flag, bits 6-0=effect block index
			 * Byte 2 : Effect operation
			 * Byte 3 : Loop count */
			_loop_count = data[3]<<3;

			switch(data[1] & 0x7F) // Effect block index
			{
				case 1: // constant force

				case 3: // square				
				case 4: // sine
					switch (data[2]) // effect operation
					{
						case EFFECT_OP_START:
							vibration_on = 1;
							decideVibration();
							break;

						case EFFECT_OP_START_SOLO:
							vibration_on = 1;
							decideVibration();
							break;	

						case EFFECT_OP_STOP:
							vibration_on = 0;
							decideVibration();
							break;
					}
					break;


				// TODO : should probably drop these from the descriptor since they are
				// not used.					

				case 2: // ramp
				case 5: // triangle
				case 6: // sawtooth up
				case 7: // sawtooth down
				case 8: // spring
				case 9: // damper
				case 10: // inertia
				case 11: // friction
				case 12: // custom force data
					break;
			}

			break;
	}

	return 1;
}

/* ------------------------------------------------------------------------- */

void transferGamepadReport(int id)
{
	if (usbInterruptIsReady())
	{ 	
		char len;
		int xfer_len;
		int j;

		len = getGamepadReport(reportBuffer, id);

		for (j=0; j<len; j+=8)
		{
			xfer_len = (len-j) < 8 ? (len-j) : 8;

			while (!usbInterruptIsReady())
			{
				usbPoll();
				wdt_reset();
			}
			usbSetInterrupt(reportBuffer+j, xfer_len);

		}
	}
}


int main(void)
{
	char must_report = 0, first_run = 1;
	uchar   idleCounters[MAX_REPORTS];
	int error_count = 0, i;

	memset(idleCounters, 0, MAX_REPORTS);

	hardwareInit();
	odDebugInit();

	/* I know both gamepads uses the same report descriptor and
	 * the same device descriptor */
	rt_usbHidReportDescriptor = gcn64_usbHidReportDescriptor;
	rt_usbHidReportDescriptorSize = getUsbHidReportDescriptor_size();
	rt_usbDeviceDescriptor = (void*)usbDescrDevice;
	rt_usbDeviceDescriptorSize = getUsbDescrDevice_size();

	// Do hardwareInit again. It causes a USB reset. 
	usbReset();
	gcn64protocol_hwinit();

	wdt_enable(WDTO_2S);
	usbInit();
	sei();
	DBG1(0x00, 0, 0);
	
	while (1) 
	{	
		wdt_reset();
	
		if (error_count > 30) {
			curGamepad = NULL;
		}

		if (curGamepad == NULL) {
			gamepadVibrate(0);
			error_count = 0;

			// this must be called at each 50 ms or less
			usbPoll();
			transferGamepadReport(1); // We know they all have only one
			_delay_ms(30);
			usbPoll();
	
			switch(gcn64_detectController())
			{
				case CONTROLLER_IS_N64:
					curGamepad = n64GetGamepad();
					curGamepad->init();
					gamepadVibrate(0);
					error_count = 0;
					break;

				case CONTROLLER_IS_GC:
					curGamepad = gamecubeGetGamepad();
					curGamepad->init();
					gamepadVibrate(0);
					error_count = 0;
					break;

					// Unknown means weird reply from the controller
					// try the old, bruteforce approach.
				case CONTROLLER_IS_UNKNOWN:
					/* Check for gamecube controller */
					curGamepad = gamecubeGetGamepad();
					curGamepad->init();
					if (curGamepad->probe()) {
						curGamepad = curGamepad;
						gamepadVibrate(0);
						error_count = 0;
						goto detected;
					}

					usbPoll();
					_delay_ms(40);
					usbPoll();

					/* Check for n64 controller */
					curGamepad = n64GetGamepad();
					curGamepad->init();
					if (curGamepad->probe())  {
						curGamepad = curGamepad;
						gamepadVibrate(0);
						error_count = 0;
						goto detected;
					}

					break;
			}
			continue;
		}

		
detected:		
		/* main event loop */
		wdt_reset();

		// this must be called at each 50 ms or less
		usbPoll();

		if (first_run) {
			decideVibration();
			if (curGamepad->update()) {
				error_count++;
			} else {
				error_count = 0;
			}
			first_run = 0;
		}

		/* Poll the controller at the configured speed */
		if (mustPollControllers())
		{
			clrPollControllers();
			
			if (!must_report)
			{
				decideVibration();

				// Wait! Before doing this, let an USB interrupt occur. This
				// prevents USB interrupts from occuring during the
				// timing sensitive Gamecube/N64 communication.
				//
				// USB communication interrupts are triggering at regular
				// intervals on my machine. Between interrupts, we have 900uS of
				// free time.
				//
				// The trick here is to put the CPU in idle mode ; That is, wating
				// for interrupts, doing nothing. When the CPU resumes, an interrupt
				// has been serviced. The final delay helps when we get more than
				// once in a row (it happens, saw it on the scope. It was inserting
				// a huge delay in the command I was sending to the controller)
				//
				sleep_enable();
				sleep_cpu();
				sleep_disable();
				_delay_us(100);

				if (curGamepad->update()) {
					error_count++;
				} else {
					error_count = 0;
				}

				/* Check what will have to be reported */
				for (i=0; i<curGamepad->num_reports; i++) {
	                if (curGamepad->changed(i+1)) {
    	                must_report |= (1<<i);
        	        }
				}
            }

		}

		if (mustRunEffectLoop()) 
		{
			clrRunEffectLoop();
			effect_loop();

		}

		if(must_report)
		{
			for (i = 0; i < curGamepad->num_reports; i++)
			{
				if ((must_report & (1<<i)) == 0)
					continue;


				transferGamepadReport(i+1);
			}

			must_report = 0;
		}
	}
	return 0;
}


/* ------------------------------------------------------------------------- */
