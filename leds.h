#ifndef _leds_h__
#define _leds_h__

#ifndef PORTD
#include <avr/io.h>
#endif


#define LED_OFF()
#define LED_ON();
#define LED_TOGGLE();

//#define LED_OFF() do { PORTD |= 0x20; } while(0)
//#define LED_ON() do { PORTD &= ~0x20; } while(0)
//#define LED_TOGGLE() do { PORTD ^= 0x20; } while(0)

#endif

