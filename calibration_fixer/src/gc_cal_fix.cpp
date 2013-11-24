#include <stdio.h>
#include <getopt.h>
#include <iostream>
#include <string>

using namespace std;

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include <dinputd.h>

LPDIRECTINPUT8          g_pDI = NULL;
LPDIRECTINPUTDEVICE8    g_pJoystick = NULL;

static int g_num_fixed = 0;
static int g_only_list = 0;
static int g_show_unsigned = 0;

int fixJoystickCalibration(void)
{
	HRESULT hr;
	DIPROPCAL cpoints;
	DIDEVCAPS devcaps;
	int x;
	int axes[6] = { DIJOFS_X, DIJOFS_Y,
					DIJOFS_RX, DIJOFS_RY, 
					DIJOFS_RZ, DIJOFS_SLIDER(0) };

	if( FAILED( hr = g_pJoystick->SetDataFormat( &c_dfDIJoystick ) ) )
		return -1;
	
	//g_pJoystick->Acquire();

	devcaps.dwSize = sizeof(devcaps);
	hr = g_pJoystick->GetCapabilities(&devcaps);
	if (FAILED(hr)) {
		//MessageBox(NULL, _com_error(GetLastError()).ErrorMessage(), L"asd", MB_ICONWARNING | MB_OK);
		printf("*** ERROR ****\n");
	}
	else {
		printf("	Axes: %lu, Buttons: %lu\n", devcaps.dwAxes, devcaps.dwButtons);
	}	

	printf("           Min.  Center  Max.\n");

	cpoints.diph.dwSize = sizeof(cpoints);
	cpoints.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	cpoints.diph.dwHow = DIPH_BYOFFSET;	

	for (x=0; x<6; x++) {
		cpoints.diph.dwObj = axes[x];
	
		hr = g_pJoystick->GetProperty(DIPROP_CALIBRATION, &cpoints.diph);
		if (FAILED(hr)) {
			continue;
		}
	
		if (g_show_unsigned) {	
			printf("	%d: %3ld    %3ld    %3ld", x, 
					cpoints.lMin, cpoints.lCenter, cpoints.lMax);
		} else {
			printf("	%d: %3ld    %3ld    %3ld", x, 
					cpoints.lMin - 127, cpoints.lCenter - 127, cpoints.lMax - 127);
		}

		if (!g_only_list) {
			if (x==4 || x==5) {
				cpoints.lCenter = cpoints.lMax;
				cpoints.lMax = 0xff;
			
				g_pJoystick->SetProperty(DIPROP_CALIBRATION, &cpoints.diph);
				printf(" (Modified)");
			}
		}
		printf("\n");
	}
	
	g_num_fixed++;

	return 0;
}


BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE*     
                                       pdidInstance, VOID* pContext)
{
    HRESULT hr;
	DIPROPDWORD vidpid;
	WORD vid, pid;

    // Obtain an interface to the enumerated joystick.
    hr = g_pDI->CreateDevice(pdidInstance->guidInstance,  
                                &g_pJoystick, NULL);
    if(FAILED(hr)) 
        return DIENUM_CONTINUE;

	/* Test */
	vidpid.diph.dwSize = sizeof(vidpid);
	vidpid.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	vidpid.diph.dwHow = DIPH_DEVICE;
	vidpid.diph.dwObj = 0;
	hr = g_pJoystick->GetProperty(DIPROP_VIDPID, &vidpid.diph);
	if (FAILED(hr)) {
		return DIENUM_CONTINUE;
	}

	vid = vidpid.dwData & 0xffff;
	pid = vidpid.dwData >> 16;

	

	if (vid==0x1781 && pid==0x0a9a) {
		printf("Found Device ID : VID=%04x  PID=%04x (ADAP-GCN64 v1.x)\n", vid, pid);
		fixJoystickCalibration(); // uses global
	}
	else if (vid==0x1740 && pid==0x057f) {
		printf("Found Device ID : VID=%04x  PID=%04x (ADAP-GCN64 v2.1)\n", vid, pid);
		fixJoystickCalibration(); // uses global
	}
	else if (vid==0x289b && pid==0x0001) {
		printf("Found Device ID : VID=%04x  PID=%04x (ADAP-GCN64 v2.2)\n", vid, pid);
		fixJoystickCalibration(); // uses global
	}
	else if (vid==0x289b && pid==0x0004) {
		printf("Found Device ID : VID=%04x  PID=%04x (ADAP-GCN64 v2.3)\n", vid, pid);
		fixJoystickCalibration(); // uses global
	}
	else if (vid==0x289b && pid==0x000c) {
		printf("Found Device ID : VID=%04x  PID=%04x (ADAP-GCN64 v2.9)\n", vid, pid);
		fixJoystickCalibration(); // uses global
	}
	else {
		printf("Ignoring Device ID : VID=%04x  PID=%04x\n", vid, pid);
		
	}

	return DIENUM_CONTINUE;	
}

int main(int argc, char * argv[])
{
	HRESULT res;
	int opt;
	int run_calibration = 0;

	printf("raphnet.net Gamecube adapter L/R buttons calibration fixer v1.3\n");
	printf("Copyright (C) 2009-2013, Raphael Assenat\n\n");	

	while(-1 != (opt = getopt(argc, argv, "hlcu"))) {
		switch (opt)
		{
			case 'h':
				printf("Usage: ./gc_calfix_ng <options>\n\n");
				printf("By default, this tool adjusts the calibration data for all supported joysticks.\n");
				printf("The following options can be used to perform other tasks.\n\n"); 
				printf("Options:\n");
				printf("  -h       Show help\n");
				printf("  -l       Only list and display supported joystick calibration settings\n");
				printf("  -c       Open the windows calibration dialog before exiting\n");
				printf("  -u       Show unsigned calibration values.\n");
				return 0;

			case 'u':
				g_show_unsigned = 1;
				break;
			
			case 'l':
				g_only_list = 1;
				break;
			
			case 'c':
				run_calibration = 1;
				break;

			default:				
				fprintf(stderr, "Unknown option passed. See -h\n");
				break;
		}	
	}


	res = DirectInput8Create(GetModuleHandle( NULL ), DIRECTINPUT_VERSION, IID_IDirectInput8, 
		(VOID **)&g_pDI, NULL);
	
	if (FAILED(res)) {
		printf("DirectInput8Create failed\n");
		return -1;
	}

	g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback,
						NULL, DIEDFL_ATTACHEDONLY);

	if (!g_num_fixed) {
		printf("No supported joysticks found.");
	}
	else {
		printf("Found %d supported joystick(s).\n", g_num_fixed);\
		if (run_calibration) {
			g_pJoystick->RunControlPanel(0,0);
		}
	}

	printf("\n -- Press ENTER to exit --\n");
	
	getchar();

	return 0;
}

