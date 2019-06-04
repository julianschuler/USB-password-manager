#ifndef __directIO__
#define __directIO__


#define conc(a, b)						(a ## b)
#define concat(a, b)					conc(a, b) //double define allows concatenante strings defined by macros

#define dRead(port, bit)				(concat(PIN, port) >> bit & 1)
#define dMode(port, bit, mode)			(mode) ? (concat(DDR, port) |= 1 << bit) : (concat(DDR, port) &= ~(1 << bit))
#define dWrite(port, bit, state)		(state) ? (concat(PORT, port) |= 1 << bit) : (concat(PORT, port) &= ~(1 << bit))

#define directRead(pin)					dRead(pin)
#define directMode(pin, mode)			dMode(pin, mode)
#define directWrite(pin, state)			dWrite(pin, state)


#endif
