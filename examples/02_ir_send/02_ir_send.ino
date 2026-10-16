// Example 02 - Midea infrared control sender (48-bit Midea AC frame @ 38 kHz)
//
// Sends the classic Midea air conditioner infrared command - the same frame
// the wired controller / handheld remote puts on the 4-wire link - so the AC
// can be controlled without touching the panel. The frame format matches the
// one decoded in Example 01.
//
// Hardware:
//   - ESP8266 (NodeMCU)
//   - IR LED on D2 (GPIO4). Direct drive (GPIO -> 100..150 ohm -> IR LED
//     -> GND) works at close range; use a driver transistor for longer range.
//     Adjust IR_LED_PIN to match your board.
//
// Frame layout (48 bits, 6 bytes):
//   [0] 0xB2 fixed header
//   [1] ~[0]
//   [2] fan code << 5 | 0x1F          (0x7B when power off)
//   [3] ~[2]
//   [4] temp code << 4 | mode << 2    (0xE0 when power off)
//   [5] ~[4]
//
// Waveform (38 kHz carrier, ~50% duty):
//   header  : 4.48 ms mark + 4.48 ms space
//   bit "1" : 0.56 ms mark + 1.68 ms space
//   bit "0" : 0.56 ms mark + 0.56 ms space
//   trailer : 0.56 ms mark + 5.6 ms gap
// The whole frame is transmitted twice per command.
//
// The demo loop cycles through a few example commands every 10 seconds:
// ON + Cool 26C (high fan) -> ON + Heat 24C (auto fan) -> ON + Fan -> OFF

#define IR_LED_PIN D2 // IR LED output pin

// ---------------------------------------------------------------------------
// Midea 38 kHz infrared timing (microseconds)
// ---------------------------------------------------------------------------

#define IR_CARRIER_FREQ 38000UL // carrier frequency, Hz

const unsigned int kMideaHdrMark = 4480;  // header mark
const unsigned int kMideaHdrSpace = 4480; // header space
const unsigned int kMideaBitMark = 560;   // bit mark
const unsigned int kMideaOneSpace = 1680; // bit "1" space
const unsigned int kMideaZeroSpace = 560; // bit "0" space
const unsigned int kMideaMinGap = 5600;   // gap after the frame

// ---------------------------------------------------------------------------
// Command encoding
// ---------------------------------------------------------------------------

// Mode values (equal to the on-air mode bits, see Example 01)
#define AC_MODE_COOL 0
#define AC_MODE_DRY 1
#define AC_MODE_AUTO 2
#define AC_MODE_HEAT 3
#define AC_MODE_FAN 4 // no temperature; encoded like Dry without a temp code

// Fan speed values (the on-air fan code, shifted into bits 7..5 of byte 2)
#define AC_FAN_HIGH 1
#define AC_FAN_MED 2
#define AC_FAN_LOW 4
#define AC_FAN_AUTO 5

static byte MideaState[6] = {0xB2, 0x4D, 0x1F, 0xE0, 0xD8, 0x27};
// known good state: ON, Auto mode, Auto fan, 26C

// Temperature (C) -> code for byte 4 (valid range 17..30 C)
unsigned char TempCodeFromC(unsigned char tempC)
{
	switch (tempC)
	{
	case 17: return 0;
	case 18: return 1;
	case 19: return 3;
	case 20: return 2;
	case 21: return 6;
	case 22: return 7;
	case 23: return 5;
	case 24: return 4;
	case 25: return 12;
	case 26: return 13;
	case 27: return 9;
	case 28: return 8;
	case 29: return 10;
	case 30: return 11;
	default: return 14; // no temperature (fan only mode)
	}
}

// Build the 48-bit command frame from the requested AC state
void MideaEncode(bool power, unsigned char mode, unsigned char fan, unsigned char tempC)
{
	if (!power)
	{
		// Power off is a fixed pattern
		MideaState[2] = 0x7B;
		MideaState[4] = 0xE0;
	}
	else
	{
		// byte 2: fan code in bits 7..5, low bits fixed at 0x1F
		MideaState[2] = (fan << 5) | 0x1F;

		// byte 4: temperature code in bits 7..4, mode in bits 3..2
		// Fan only mode is encoded like Dry (mode bits = 1) without a
		// temperature code
		unsigned char modeBits = (mode == AC_MODE_FAN) ? AC_MODE_DRY : mode;
		unsigned char tempCode = (mode == AC_MODE_FAN) ? 0x0E : TempCodeFromC(tempC);
		MideaState[4] = (tempCode << 4) | (modeBits << 2);
	}

	// Complement bytes
	MideaState[1] = (byte)(~(MideaState[0]));
	MideaState[3] = (byte)(~(MideaState[2]));
	MideaState[5] = (byte)(~(MideaState[4]));
}

// ---------------------------------------------------------------------------
// 38 kHz infrared waveform generation
// ---------------------------------------------------------------------------

// LED on for the given time, modulated with the 38 kHz carrier (~50% duty)
void IrMark(unsigned int usec)
{
	const unsigned long period = 1000000UL / IR_CARRIER_FREQ; // 26 us @ 38 kHz
	const unsigned long onTime = period / 2;                  // 50% duty
	const unsigned long offTime = period - onTime;
	const unsigned long start = micros();
	unsigned long elapsed = 0;

	while (elapsed < usec)
	{
		digitalWrite(IR_LED_PIN, HIGH);
		delayMicroseconds(min(onTime, usec - elapsed));
		digitalWrite(IR_LED_PIN, LOW);
		if (elapsed + onTime >= usec)
		{
			return;
		}
		delayMicroseconds(min(usec - elapsed - onTime, offTime));
		elapsed = micros() - start;
	}
}

// LED off for the given time
void IrSpace(unsigned long usec)
{
	digitalWrite(IR_LED_PIN, LOW);
	if (usec == 0)
	{
		return;
	}
	delayMicroseconds(usec);
}

// Send one byte, MSB first
void IrSendByte(byte value)
{
	for (byte mask = 0x80; mask != 0; mask >>= 1)
	{
		IrMark(kMideaBitMark);
		if (value & mask)
		{
			IrSpace(kMideaOneSpace);
		}
		else
		{
			IrSpace(kMideaZeroSpace);
		}
	}
}

// Send the whole frame (transmitted twice per command, like the remote does)
void MideaIrSendFrame()
{
	for (byte frame = 0; frame < 2; frame++)
	{
		IrMark(kMideaHdrMark);
		IrSpace(kMideaHdrSpace);
		for (byte i = 0; i < 6; i++)
		{
			IrSendByte(MideaState[i]);
		}
		IrMark(kMideaBitMark);
		IrSpace(kMideaMinGap);
	}
}

// ---------------------------------------------------------------------------

void PrintState(const char *action)
{
	Serial.printf("Send: %s [raw: %02X %02X %02X %02X %02X %02X]\r\n"
		, action
		, MideaState[0], MideaState[1], MideaState[2]
		, MideaState[3], MideaState[4], MideaState[5]);
}

void setup()
{
	delay(50);
	Serial.begin(115200);
	Serial.println("Midea IR sender start");

	pinMode(IR_LED_PIN, OUTPUT);
	digitalWrite(IR_LED_PIN, LOW);

	Serial.println("Ready");
}

void loop()
{
	// Demo: cycle through a few example commands, 10 seconds apart
	static unsigned char Step = 0;

	switch (Step)
	{
	case 0:
		MideaEncode(true, AC_MODE_COOL, AC_FAN_HIGH, 26);
		PrintState("ON, Cool 26C, High fan");
		break;

	case 1:
		MideaEncode(true, AC_MODE_HEAT, AC_FAN_AUTO, 24);
		PrintState("ON, Heat 24C, Auto fan");
		break;

	case 2:
		MideaEncode(true, AC_MODE_FAN, AC_FAN_AUTO, 0);
		PrintState("ON, Fan only, Auto fan");
		break;

	case 3:
		MideaEncode(false, AC_MODE_AUTO, AC_FAN_AUTO, 0);
		PrintState("OFF");
		break;
	}
	Step = (Step + 1) & 3;

	MideaIrSendFrame();

	delay(10000);
}
