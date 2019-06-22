#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdint.h>

#include <usbdrv.h>
#include <keycodes.h>
#include <directIO.h>


#define PASSWORD_LENGTH			100			// length of the generated and stored password
#define SEND_RETURN				1			// set to 1 to send a return after the password has been typed
#define PASSWORD_VISIBLE		0			// set to 1 to show the password during generation (not recommended)
#define HEX_LETTERS_CAPITAL		1			// set to 1 to use capital letters in the password
#define LANGUAGE				EN			// sets the language, available options: EN, DE
#define BUTTON_PRESS_TIME_MS	2500		// mimimum time the button has to be pressed for generating a new password

#if LANGUAGE == DE
	#define CONF_MSG			"Drücken Sie vier Mal Caps Lock, um die Generierung eines neuen Passwortes zu bestätigen.\n"
	#define INIT_MSG			"Generiere neues Passwort...\n"
	#define DONE_MSG			"\nFertig!"
	#define LAYOUT				LAYOUT_DE
#else
	#define CONF_MSG			"Toggle Caps Lock four times to confirm the generation of a new password."
	#define INIT_MSG			"Generating new password...\n"
	#define DONE_MSG			"\nDone!"
	#define LAYOUT				LAYOUT_US
#endif


#define BUTTON_PIN 				B,1
#define OSCCAL_INDEX			(uint8_t*)(0)
#define IS_CAL_INDEX			(uint8_t*)(1)
#define TIMER_COMP_VAL			(F_CPU / 262144 * BUTTON_PRESS_TIME_MS / 1000)

#define abs(x) 					((x) > 0 ? (x) : (-x))
#define ustrlen_P(str)			(sizeof(str) - 1)
#define setConfWDT()			(MCUSR = 0)
#define beginWriteWDT()			(WDTCR |= (1<<WDCE) | (1<<WDE))
#define setConfTimer0()			(TCCR0A = 0)
#define enableTimer0()			(TCCR0B = (1<<CS00) | (1<<CS02))
#define disableTimer0()			(TCCR0B = 0)
#define enableTimer1()			(TCCR1 = (1<<CS10))
#define disableTimer1()			(TCCR1 = 0)
#define enableOverflowINT()		(TIMSK = (1<<TOIE0))
#define disableOverflowINT()	(TIMSK = 0)


PROGMEM const unsigned char confMsg[] = CONF_MSG;
PROGMEM const unsigned char initMsg[] = INIT_MSG;
PROGMEM const unsigned char doneMsg[] = DONE_MSG;

uint8_t idle_rate = 500 / 4; /* see HID1_11.pdf sect 7.2.4 */
uint8_t protocol_version = 0; /* see HID1_11.pdf sect 7.2.6 */

volatile uint8_t LED_states = 0; /* see HID1_11.pdf appendix B section 1 */
volatile uint8_t capsLockToggled = 0;
volatile uint8_t sample = 0;
volatile uint8_t newSample = 0;
volatile uint8_t TCNT0H = 0;

uint8_t	timer0Active = 0;
uint8_t password[PASSWORD_LENGTH];


PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
	0x05, 0x01,			/* USAGE_PAGE (Generic Desktop)						*/
	0x09, 0x06,			/* USAGE (Keyboard)									*/
	0xa1, 0x01,			/* COLLECTION (Application)							*/
	0x75, 0x01,			/* REPORT_SIZE (1)									*/
	0x95, 0x08,			/* REPORT_COUNT (8)									*/
	0x05, 0x07,			/* USAGE_PAGE (Keyboard)(Key Codes)					*/
	0x19, 0xe0,			/* USAGE_MINIMUM (Keyboard LeftControl)(224)		*/
	0x29, 0xe7,			/* USAGE_MAXIMUM (Keyboard Right GUI)(231)			*/
	0x15, 0x00,			/* LOGICAL_MINIMUM (0)								*/
	0x25, 0x01,			/* LOGICAL_MAXIMUM (1)								*/
	0x81, 0x02,			/* INPUT (Data,Var,Abs) ; Modifier byte				*/
	0x95, 0x01,			/* REPORT_COUNT (1)									*/
	0x75, 0x08,			/* REPORT_SIZE (8)									*/
	0x81, 0x03,			/* INPUT (Cnst,Var,Abs) ; Reserved byte				*/
	0x95, 0x05,			/* REPORT_COUNT (5)									*/
	0x75, 0x01,			/* REPORT_SIZE (1)									*/
	0x05, 0x08,			/* USAGE_PAGE (LEDs)								*/
	0x19, 0x01,			/* USAGE_MINIMUM (Num Lock)							*/
	0x29, 0x05,			/* USAGE_MAXIMUM (Kana)								*/
	0x91, 0x02,			/* OUTPUT (Data,Var,Abs) ; LED report				*/
	0x95, 0x01,			/* REPORT_COUNT (1)									*/
	0x75, 0x03,			/* REPORT_SIZE (3)									*/
	0x91, 0x03,			/* OUTPUT (Cnst,Var,Abs) ; LED report padding		*/
	0x95, 0x06,			/* REPORT_COUNT (6)									*/
	0x75, 0x08,			/* REPORT_SIZE (8)									*/
	0x15, 0x00,			/* LOGICAL_MINIMUM (0)								*/
	0x25, 0x65,			/* LOGICAL_MAXIMUM (101)							*/
	0x05, 0x07,			/* USAGE_PAGE (Keyboard)(Key Codes)					*/
	0x19, 0x00,			/* USAGE_MINIMUM (Reserved (no event indicated))(0)	*/
	0x29, 0x65,			/* USAGE_MAXIMUM (Keyboard Application)(101)		*/
	0x81, 0x00,			/* INPUT (Data,Ary,Abs)								*/
	0xc0				/* END_COLLECTION									*/
};


void sendReport(uint8_t modifiers, uint8_t keycode1, uint8_t keycode2, uint8_t keycode3, uint8_t keycode4, uint8_t keycode5, uint8_t keycode6) {
	while (!usbInterruptIsReady()) usbPoll();
	uint8_t report_buffer[8] = {modifiers, 0, keycode1, keycode2, keycode3, keycode4, keycode5, keycode6};
	usbSetInterrupt(report_buffer, sizeof(report_buffer));
}


void sendKey(uint8_t keycode, uint8_t modifiers) {
	if (keycode != 0) {
		sendReport(modifiers, keycode, 0, 0, 0, 0, 0);
		sendReport(0, 0, 0, 0, 0, 0, 0);
	}
}


uint8_t asciiToKeycode(unsigned char ascii) {
	if (ascii >= ' ' && ascii <= '~') {
		return pgm_read_byte_near(concat(KEYCODES_, LAYOUT) + ascii - ' ');
	}
	
	switch(ascii) {
		case '\t':
			return 0x2B;
		case '\n':
			return 0x28;
		#if LAYOUT == LAYOUT_DE
		case 0x84:	/* Ä */
		case 0xA4:	/* ä */
			return 0x34;
		case 0x96:	/* Ö */
		case 0xB6:	/* ö */
			return 0x33;
		case 0x9C:	/* Ü */
		case 0xBC:	/* ü */
			return 0x2F;
		case 0x9F:	/* ß */
			return 0x2D;
		#endif
	}
	
	return 0;
}


uint8_t asciiToShiftState(unsigned char ascii) {
	if (ascii >= ' ' && ascii <= '~') {
		if ((pgm_read_byte_near(concat(MODIFIER_SHIFT_, LAYOUT) + 11) & 1) || (ascii >= 'a' && ascii <= 'z') || (ascii >= 'A' && ascii <= 'Z')) {
			return (LED_states ^ ((pgm_read_byte_near(concat(MODIFIER_SHIFT_, LAYOUT) + (ascii - ' ')/8) >> (7 - ((ascii - ' ') % 8))) << 1)) & 2;
		}
		else {
			return ((pgm_read_byte_near(concat(MODIFIER_SHIFT_, LAYOUT) + (ascii - ' ')/8) >> (7 - ((ascii - ' ') % 8))) << 1) & 2;
		}
	}
	
	#if LAYOUT == LAYOUT_DE
	switch(ascii) {
		case 0x84:	/* Ä */
		case 0x96:	/* Ö */
		case 0x9C:	/* Ü */
			return (LED_states & 2) ^ 2;
		case 0xA4:	/* ä */
		case 0xB6:	/* ö */
		case 0xBC:	/* ü */
		case 0x9F:	/* ß */
			return LED_states & 2;
	}
	#endif
	
	/* char unknown or \t or \n, they need no Shift compensation for Caps Lock */
	return 0;
}


char hexToAscii(uint8_t b) {
	b = b & 0x0F;
	if (b < 0x0A) {
		return (b + '0');
	}
	else {
		#if HEX_LETTERS_CAPITAL
		return (b - 0x0A + 'A');
		#else
		return (b - 0x0A + 'a');
		#endif
	}
}


void write(unsigned char ascii) {
	while (!usbInterruptIsReady()) usbPoll();
	sendKey(asciiToKeycode(ascii), asciiToShiftState(ascii));
}


void printPStr(const unsigned char* str, uint8_t len) {
	for (uint8_t i = 0; i < len; i++) {
		write(pgm_read_byte_near(str + i));
	}
}


void debugPStr(const unsigned char* str, uint8_t len) {
	for (uint8_t i = 0; i < len; i++) {
		uint8_t ascii = pgm_read_byte_near(str + i);
		write(hexToAscii(ascii>>4));
		write(hexToAscii(ascii));
		write('\n');
	}
}


uint8_t rotateLeft(const uint8_t value, uint8_t shift) {
	if ((shift &= sizeof(value)*8 - 1) == 0) {
		return value;
	}
	return (value << shift) | (value >> (sizeof(value)*8 - shift));
}


void enableWDT() {
	cli();
	beginWriteWDT();
	WDTCR = (1<<WDIE);
	sei();
}


void disableWDT() {
	cli();
	beginWriteWDT();
	WDTCR = 0;
	sei();
}


void generateNewPassword() {
	uint8_t currentBit = 0;
	uint8_t result = 0;
	printPStr(confMsg, ustrlen_P(confMsg));
	while (capsLockToggled < 4) usbPoll();
	capsLockToggled -= 4;
	printPStr(initMsg, ustrlen_P(initMsg));
	enableWDT();
	for (uint16_t i = 0; i < PASSWORD_LENGTH; i++) {
		currentBit = 0;
		while (currentBit < 4) {
			usbPoll();
			if (newSample) {
				newSample = 0;
				result = rotateLeft(result, 1); // Spread randomness around
				result ^= sample; // XOR preserves randomness
				currentBit++;
			}
		}
		result &= 0x0F;
		password[i] = result;
		eeprom_busy_wait();
		eeprom_write_byte((uint8_t*)(i+2), result);
		#if PASSWORD_VISIBLE
		write(hexToAscii(result));
		#else
		write('*');
		#endif
	}
	disableWDT();
	printPStr(doneMsg, ustrlen_P(doneMsg));
}


void readPassword() {
	for (uint16_t i = 0; i < PASSWORD_LENGTH; i++) {
		eeprom_busy_wait();
		password[i] = eeprom_read_byte((uint8_t*)(i+2));
		usbPoll();
	}
}


void printPassword() {
	for (uint16_t i = 0; i < PASSWORD_LENGTH; i++) {
		write(hexToAscii(password[i]));
	}
	#if SEND_RETURN
	write('\n');
	#endif
}


int main() {
	cli();
	usbInit();
	setConfTimer0();
	setConfWDT();
	eeprom_busy_wait();
	if (eeprom_read_byte(IS_CAL_INDEX) == 1) {
		eeprom_busy_wait();
		OSCCAL = eeprom_read_byte(OSCCAL_INDEX);
	}
	
	sei();
	usbPoll();
	readPassword();
	
	while(1) {
		if (directRead(BUTTON_PIN) == 0) {
			if (timer0Active == 0) {
				enableOverflowINT();
				enableTimer0();
				timer0Active = 1;
			}
			if (TCNT0H >= TIMER_COMP_VAL) {
				disableOverflowINT();
				disableTimer0();
				timer0Active = 0;
				enableTimer1();
				while (directRead(BUTTON_PIN) == 0) {
					usbPoll();
				}
				generateNewPassword();
				disableTimer1();
			}
		}
		else if (timer0Active) {
			disableOverflowINT();
			disableTimer0();
			timer0Active = 0;
		}
		if ((capsLockToggled & 1) == 0 && capsLockToggled) {
			capsLockToggled = 0;
			printPassword();
		}
		usbPoll();
	}
	
	return 0;
}


/* Watchdog Timer Interrupt Service Routine */
ISR(WDT_vect) {
	sample = TCNT1;
	newSample = 1;
}


/* Timer0 Overflow Interrupt Service Routine */
ISR(TIMER0_OVF_vect) {
	TCNT0H++;
}


/*##################################### V-USB FUNCTIONS ######################################*/

/* declare the USB device as HID keyboard */
usbMsgLen_t usbFunctionSetup(uint8_t data[8]) {
	/* see HID1_11.pdf sect 7.2 and http://vusb.wikidot.com/driver-api */
	usbRequest_t *rq = (void *)data;
	
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_CLASS)
		return 0; /* ignore request if it's not a class specific request */
	
	/* see HID1_11.pdf sect 7.2 */
	switch (rq->bRequest) {
		case USBRQ_HID_GET_IDLE:
			usbMsgPtr = &idle_rate; /* send data starting from this byte */
			return 1; /* send 1 byte */
		case USBRQ_HID_SET_IDLE:
			idle_rate = rq->wValue.bytes[1]; /* read in idle rate */
			return 0; /* send nothing */
		case USBRQ_HID_GET_PROTOCOL:
			usbMsgPtr = &protocol_version; /* send data starting from this byte */
			return 1; /* send 1 byte */
		case USBRQ_HID_SET_PROTOCOL:
			protocol_version = rq->wValue.bytes[1];
			return 0; /* send nothing */
		case USBRQ_HID_SET_REPORT:
			if (rq->wLength.word == 1) { /* check data is available
				1 byte, we don't check report type (it can only be output or feature)
				we never implemented "feature" reports so it can't be feature
				so assume "output" reports
				this means set LED status
				since it's the only one in the descriptor */
				return USB_NO_MSG; /* send nothing but call usbFunctionWrite */
			}
		default: /* do not understand data, ignore */
			return 0; /* send nothing */
	}
}


/* detect LED states (Caps Lock, Num Lock, Scroll Lcok) */
usbMsgLen_t usbFunctionWrite(uint8_t* data, uchar len) {
	if (data[0] == LED_states) { /* return, if no LED states have changed */
		return 1;
	}
	LED_states = data[0];
	if (LED_states & 2) {
		if ((capsLockToggled & 1) == 0) {
			capsLockToggled++;
		}
	}
	else if (capsLockToggled & 1) {
		capsLockToggled++;
	}
	return 1; /* Data read, not expecting more */
}


/* calibrate OSCCAL */
void usbEventResetReady() {
	cli();
	eeprom_busy_wait();
	if (eeprom_read_byte(IS_CAL_INDEX) == 1) {
		eeprom_busy_wait();
		OSCCAL = eeprom_read_byte(OSCCAL_INDEX);
	}
	else {
		uint16_t frameLength;
		uint16_t targetLength = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
		uint16_t bestDeviation = 9999;
		uint8_t trialCal;
		uint8_t bestCal = 0;
		uint8_t step;
		uint8_t region;

		/* do a binary search in regions 0-127 and 128-255 to get optimum OSCCAL */
		for (region = 0; region <= 1; region++) {
			frameLength = 0;
			trialCal = (region == 0) ? 0 : 128;
			
			for (step = 64; step > 0; step >>= 1) { 
				if(frameLength < targetLength) {
					trialCal += step;
				}
				else {
					trialCal -= step;
				}
				
				OSCCAL = trialCal;
				frameLength = usbMeasureFrameLength();
				
				if (abs(frameLength-targetLength) < bestDeviation) {
					bestCal = trialCal;
					bestDeviation = abs(frameLength-targetLength);
				}
			}
		}
		
		eeprom_busy_wait();
		eeprom_write_byte(OSCCAL_INDEX, bestCal);
		eeprom_busy_wait();
		eeprom_write_byte(IS_CAL_INDEX, 1);
		OSCCAL = bestCal;
	}
	sei();
}
