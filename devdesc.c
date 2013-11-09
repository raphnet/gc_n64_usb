#include "devdesc.h"
#include "usbconfig.h"

#define USBDESCR_DEVICE         1

const char usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    USB_CFG_DEVICE_ID,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
    1,          /* manufacturer string index */
    2,          /* product string index */
    3,          /* serial number string index */
    1,          /* number of configurations */
};

int getUsbDescrDevice_size(void) { return sizeof(usbDescrDevice); }

