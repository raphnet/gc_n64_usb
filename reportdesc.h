#ifndef _reportdesc_h__
#define _reportdesc_h__

#include <avr/pgmspace.h>

#define GCN64_REPORT_SIZE	9

extern const char gcn64_usbHidReportDescriptor[] PROGMEM;
int getUsbHidReportDescriptor_size(void);

#endif // _reportdesc_h__

