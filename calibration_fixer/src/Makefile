CC=gcc
CPP=g++
LD=$(CPP)

DX_INCLUDE_PATH=/c/dx9sdk/Include
DX_LIBS=/c/dx9sdk/Lib/x86/dinput8.lib /c/dx9sdk/Lib/x86/dxguid.lib

CFLAGS=-Wall -I$(DX_INCLUDE_PATH)
CPPFLAGS=$(CFLAGS)
LDFLAGS=$(DX_LIBS) -static

gc_calfix_ng: gc_cal_fix.o
	$(LD) $^ -o $@ $(LDFLAGS)

clean:
	rm gc_calfix_ng.exe
