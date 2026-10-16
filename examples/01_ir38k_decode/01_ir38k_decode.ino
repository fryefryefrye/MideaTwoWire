// Example 01 - Midea 38 kHz infrared control signal decoder (4-wire controllers)
//
// Older Midea wired controllers are 4-wire: the control board sends a 38 kHz
// modulated infrared control signal to the indoor unit over the wiring, and
// the controller itself also receives the handheld remote. This sketch
// passively taps that signal and decodes the 48-bit Midea infrared frame,
// so the commanded AC state (power / mode / set temperature / fan speed)
// can be monitored.
//
// Hardware:
//   - ESP8266 (NodeMCU)
//   - a demodulating 38 kHz IR receiver (e.g. TSOP1838 / VS1838B) whose input
//     is the IR signal line tapped from the controller wiring
//
// Wiring:
//   IR receiver OUT  ->  D6 (GPIO12), input with pull-up, active low
//   Serial monitor   ->  115200 baud
//
// Signal timing (measured at the demodulated receiver output):
//   header  : mark > 3.8 ms + space > 3.8 ms
//   bit "1" : space > 1.0 ms  (1.68 ms)
//   bit "0" : space < 1.0 ms  (0.56 ms)
// Each command is transmitted twice (two identical 48-bit frames).
//
// Frame layout as captured here (6 bytes):
//   [0] 0xB2 fixed header
//   [1] ~[0]
//   [2] fan code << 5 | 0x1F          (0x7B when power off)
//   [3] ~[2]
//   [4] temp code << 4 | mode << 2    (0xE0 when power off)
//   [5] ~[4]
//
// See Example 02 for the matching sender.

#define IR_RX_PIN D6 // 38 kHz IR signal input (demodulated IR receiver output)

// ---------------------------------------------------------------------------
// Frame capture (interrupt driven)
// ---------------------------------------------------------------------------

#define SAMPLE_NUMBER 200

static byte IrData1[6] = {0, 0, 0, 0, 0, 0}; // 1st frame of a command
static byte IrData2[6] = {0, 0, 0, 0, 0, 0}; // 2nd frame of a command
static byte *pIrData = IrData1;

static unsigned long PulseStartTime = 0;
static unsigned long LastTime = 0;
static volatile unsigned long LastEdgeTime = 0; // shared with loop()
static bool IsLastLong = true;
static volatile unsigned char BitCounter = 0;   // shared with loop()
static bool IsCommandStart = false;

// Set by the ISR, read (and cleared) in loop()
static volatile bool FrameReady = false;
static volatile bool Frame1Captured = false;

void ResetIrCapture();

// 38 kHz infrared signal decode interrupt (RISING edges of the demodulated
// receiver output: a rising edge ends each mark)
ICACHE_RAM_ATTR void IrDecodeIsr()
{
	unsigned long ThisTime;
	unsigned long DiffTime;
	unsigned int PulseTime; // mark (low) duration, us
	unsigned int IdleTime;  // space (high) duration, us

	if (BitCounter < SAMPLE_NUMBER)
	{
		if (LastTime == 0) // record the first timestamp
		{
			LastTime = micros();
		}
		else
		{
			ThisTime = micros();
			DiffTime = ThisTime - LastTime;
			LastEdgeTime = ThisTime;

			if (DiffTime < 100)
			{
				// Very short interval: glitch, restart the mark measurement
				if (IsLastLong)
				{
					PulseStartTime = ThisTime;
					IsLastLong = false;
				}
			}
			else
			{
				PulseTime = LastTime - PulseStartTime; // mark duration
				IdleTime = DiffTime;                   // space duration

				if (IsCommandStart)
				{
					if (IdleTime > 3800)
					{
						// Header of the next frame: nothing to store
					}
					else if (!FrameReady)
					{
						// Data bit: long space = 1, short space = 0
						if (IdleTime > 1000)
						{
							pIrData[BitCounter / 8] += 1 << (7 - BitCounter % 8);
						}
						// else: bit 0
						BitCounter++;
						if (BitCounter >= 48)
						{
							if (pIrData == IrData1)
							{
								// 1st frame done. The command is always sent
								// twice: capture the 2nd copy into IrData2
								BitCounter = 0;
								IsCommandStart = false;
								pIrData = IrData2;
								Frame1Captured = true;
							}
							else
							{
								FrameReady = true; // both frames captured
							}
						}
					}
				}
				else
				{
					// Long mark + long space = frame header
					if ((PulseTime > 3800) && (IdleTime > 3800))
					{
						IsCommandStart = true;
					}
				}
				IsLastLong = true;
			}
			LastTime = ThisTime;
		}
	}
}

void ResetIrCapture()
{
	noInterrupts();
	for (unsigned char i = 0; i < 6; i++)
	{
		IrData1[i] = 0;
		IrData2[i] = 0;
	}
	pIrData = IrData1;
	BitCounter = 0;
	LastTime = 0;
	PulseStartTime = 0;
	IsLastLong = true;
	IsCommandStart = false;
	Frame1Captured = false;
	LastEdgeTime = micros();
	interrupts();
}

// ---------------------------------------------------------------------------
// Frame validation and decoding
// ---------------------------------------------------------------------------

bool IsFrameValid(const byte *p)
{
	return (p[0] == 0xB2)
		&& (p[0] == (byte)(~(p[1])))
		&& (p[2] == (byte)(~(p[3])))
		&& (p[4] == (byte)(~(p[5])));
}

// Temperature code -> degrees C (0 = no temperature set / invalid).
// The code is the high nibble of byte 4.
unsigned char IrTempFromCode(unsigned char code)
{
	switch (code)
	{
	case 0: return 17;
	case 1: return 18;
	case 3: return 19;
	case 2: return 20;
	case 6: return 21;
	case 7: return 22;
	case 5: return 23;
	case 4: return 24;
	case 12: return 25;
	case 13: return 26;
	case 9: return 27;
	case 8: return 28;
	case 10: return 29;
	case 11: return 30;
	case 14: // fan only mode: no temperature
	default:
		return 0;
	}
}

void PrintHexFrame(const byte *p)
{
	for (unsigned char i = 0; i < 6; i++)
	{
		Serial.printf("%02X ", p[i]);
	}
	Serial.println();
}

void PrintAcState(const byte *p)
{
	// Power off is a fixed pattern: byte2 = 0x7B, byte4 = 0xE0
	if (p[2] == 0x7B && p[4] == 0xE0)
	{
		Serial.println("AC : [OFF]");
		return;
	}

	// Temperature: high nibble of byte 4
	unsigned char setTemp = IrTempFromCode(p[4] >> 4);

	// Code 0xE (decoded as 0) means "no temperature set" = fan only mode.
	// p[2] == 0xBF (fan code = auto) filters out swing-only frames.
	if (setTemp == 0)
	{
		if (p[2] == 0xBF)
		{
			Serial.println("AC : [ON] [Mode:Fan]");
		}
		// else: swing command or noise, not a state change
		return;
	}

	// Mode: bits 3..2 of byte 4
	const char *modeStr = "Unknown";
	switch ((p[4] >> 2) & 3)
	{
	case 0: modeStr = "Cool"; break;
	case 1: modeStr = "Dry"; break;
	case 2: modeStr = "Auto"; break;
	case 3: modeStr = "Heat"; break;
	}

	// Fan: bits 7..5 of byte 2
	const char *fanStr = "Unknown";
	switch ((p[2] >> 5) & 7)
	{
	case 1: fanStr = "High"; break;
	case 2: fanStr = "Med"; break;
	case 4: fanStr = "Low"; break;
	case 5: fanStr = "Auto"; break;
	}

	Serial.printf("AC : [ON] [Mode:%s] [Set:%uC] [Fan:%s]\r\n", modeStr, setTemp, fanStr);
}

void HandleIrFrames()
{
	// The same command is always transmitted twice. Print both raw frames,
	// then decode the 2nd copy, falling back to the 1st one.

	Serial.print("IR 1: ");
	PrintHexFrame(IrData1);
	Serial.print("IR 2: ");
	PrintHexFrame(IrData2);

	if (IsFrameValid(IrData2))
	{
		PrintAcState(IrData2);
	}
	else if (IsFrameValid(IrData1))
	{
		PrintAcState(IrData1);
	}
	else
	{
		Serial.println("IR frame corrupted, dropped");
	}
}

// ---------------------------------------------------------------------------

void setup()
{
	delay(50);
	Serial.begin(115200);
	Serial.println("Midea 38 kHz IR decoder start");

	// IR signal input: demodulated IR receiver output, active low
	pinMode(IR_RX_PIN, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(IR_RX_PIN), IrDecodeIsr, RISING);

	Serial.println("Ready");
}

void loop()
{
	if (FrameReady)
	{
		FrameReady = false;
		HandleIrFrames();
		ResetIrCapture();
	}
	else if ((BitCounter != 0 || Frame1Captured)
		&& (micros() - LastEdgeTime > 400000))
	{
		// Partial capture (noise or aborted transmission): drop it and
		// wait for the next header
		ResetIrCapture();
	}
}
