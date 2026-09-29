// Two-wire bus AC status parser (simplified): parse the two-wire bus signal and print
// the decoded AC status over the hardware serial port. All other features (WiFi/UDP
// reporting, OTA, NTP time, 38 kHz IR TX/RX, sensor libraries) have been removed.
//
// D0 = GPIO16; no INPUT_PULLUP, use external pull-up
// D1 = GPIO5;
// D2 = GPIO4
// D3 = GPIO0;can not download when connected to low
// D4 = GPIO2;	 LED on esp8266 can not start when low input
// D5 = GPIO14;
// D6 = GPIO12;
// D7 = GPIO13;
// D8 = GPIO15;  can not start when high input
// D9 = GPIO3; UART RX
// D10 = GPIO1; UART TX
// LED_BUILTIN = GPIO16 (auxiliary constant for the board LED, not a board pin);

#define MIDEA_AC_RX D6 // Two-wire bus signal input (software serial RX, 4800 baud)

#include <SoftwareSerial.h>

SoftwareSerial *p_swSer_TwoWire = nullptr;

void TwoWireTask();
void ParseTwoWireAcPacket(unsigned char *pBuf, unsigned int len);

// Midea two-wire / XYE protocol 16-bit CRC (CRC-16/MODBUS: Poly=0xA001, Init=0xFFFF)
// Calculation range: from the 2nd byte (skipping the 0xAA frame header) to just
// before the CRC field (len - 5 bytes in total)
uint16_t CalTwoWireCrc16(const unsigned char *pBuf, unsigned int len)
{
	uint16_t crc = 0xFFFF;
	for (unsigned int i = 1; i < len - 4; i++)
	{
		crc ^= pBuf[i];
		for (unsigned char b = 0; b < 8; b++)
		{
			if (crc & 1)
			{
				crc = (crc >> 1) ^ 0xA001;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}

// Two-wire packet validation: verify frame header, frame tail, length and CRC16
bool VerifyTwoWirePacket(const unsigned char *pBuf, unsigned int len)
{
	// 1. Minimum length limit (AA + Type + Addr[4] + DataLen + Check[2] + 55 FE = 11 bytes)
	if (len < 11 || pBuf == nullptr)
	{
		return false;
	}

	// 2. Frame header must be 0xAA
	if (pBuf[0] != 0xAA)
	{
		return false;
	}

	// 3. Frame tail must be 0x55 0xFE
	if (pBuf[len - 2] != 0x55 || pBuf[len - 1] != 0xFE)
	{
		return false;
	}

	// 4. Strict payload/total length check: len == DataLen + 11
	unsigned int dataLen = pBuf[6];
	if (len != (dataLen + 11))
	{
		return false;
	}

	// 5. CRC16 check (the 4th and 3rd bytes from the end are the CRC field,
	//    low byte first, high byte second)
	uint16_t calcCrc = CalTwoWireCrc16(pBuf, len);
	unsigned char crcLow = calcCrc & 0xFF;
	unsigned char crcHigh = (calcCrc >> 8) & 0xFF;

	if (pBuf[len - 4] != crcLow || pBuf[len - 3] != crcHigh)
	{
		return false;
	}

	return true;
}

// [0]     : 0xAA (frame header)
// [1]     : Protocol family (e.g. 0x23 / 0x20 / 0x70 / 0x71 / 0x76)
// [2..5]  : Source address and destination address (4 bytes)
// [6]     : Payload length (DataLen)
// [7..N-5]: Business data segment (DataLen bytes)
// [N-4..N-3]: 2-byte checksum (Checksum / CRC16)
// [N-2..N-1]: 0x55 0xFE (frame tail)

// Precise command-level matching of the indoor unit status frame:
// 1. Frame header fixed at 0xAA
// 2. Frame tail fixed at 0x55 0xFE
// 3. Command family 0x23 (indoor unit communication)
// 4. Source address 0xF8 (indoor unit main board)
// 5. Function code 0x65 (indoor unit operating parameters / status report response)
// 6. Payload length pBuf[6] >= 10

void ParseTwoWireAcPacket(unsigned char *pBuf, unsigned int len)
{
	// 1. Validate the whole frame including the checksum: drop it on failure
	if (!VerifyTwoWirePacket(pBuf, len))
	{
		return;
	}

	// 2. Match the indoor unit status response command (AA 23 F8 ... 65 [Data] ... Check 55 FE)
	if (pBuf[1] != 0x23 || pBuf[2] != 0xF8 || pBuf[7] != 0x65 || pBuf[6] < 10)
	{
		return;
	}

	// Make sure the payload length is valid
	unsigned int payloadLen = pBuf[6];
	if (payloadLen > 20)
	{
		payloadLen = 20;
	}

	unsigned char *pData = &pBuf[8];

	static unsigned char LastPayload[20] = {0};
	static bool HasFirstPayload = false;

	// Check whether any data has changed
	bool changed = false;
	if (!HasFirstPayload)
	{
		changed = true;
		HasFirstPayload = true;
	}
	else
	{
		for (unsigned int i = 0; i < payloadLen; i++)
		{
			if (pData[i] != LastPayload[i])
			{
				changed = true;
				break;
			}
		}
	}

	// Save the latest payload for the next comparison
	for (unsigned int i = 0; i < payloadLen; i++)
	{
		LastPayload[i] = pData[i];
	}

	unsigned char b0 = pData[0]; // 0x42 (mode / power flag)
	unsigned char b1 = pData[1]; // 0x80 / 0x81
	unsigned char b2 = pData[2]; // Set temperature
	unsigned char b3 = pData[3]; // Fan speed
	unsigned char b4 = pData[4]; // Swing / auxiliary status
	unsigned char b9 = pData[9]; // Indoor temperature (0x35 = 53 -> 53-30 = 23 C)

// 0x00: power off (Bit 6 = 0)
// 0x42: power on + set to cooling mode
// 0x46: power on + set to dry mode
// 0x41: power on + set to fan mode
// 0x43: power on + set to heat mode
// 0xC0: power on + set to auto mode, actually running in cooling
//       (Bit 7 = 1 actual cooling, Bit 6 = 1 power on, low 4 bits = 0 auto)

	// Power state: bit 6 (0x40) or bit 7 (0x80) of b0 means power on (e.g. 0x42/0x46/0xC0)
	bool isPowerOn = (b0 & 0xC0) != 0;

	// Running mode decoding: Midea standard low 4 bits / mask
	// (0x42->2 cooling, 0x46->6 dry, 0xC0->0 auto, 3 heat, 1 fan, etc.)
	unsigned char rawMode = b0 & 0x0F;
	const char *modeStr = "Unknown";
	switch (rawMode)
	{
	case 0: modeStr = "Auto"; break;
	case 1: modeStr = "Fan"; break;
	case 2: modeStr = "Cool"; break;
	case 3: modeStr = "Heat"; break;
	case 6: modeStr = "Dry"; break;
	default: modeStr = "Other"; break;
	}

	// Set temperature decoding:
	//unsigned char setTemp = (b2 & 0x1F)/2-40;
	//unsigned char setTemp = (b2)/2-40;
	unsigned char setTemp = ((b2 & 0xFE) >> 1) - 40;

	// Fan speed decoding: b1 (0x80 = auto fan, 0x01~0x07 = set level 1~7)
	char fanStr[16];
	if (b1 == 0x80)
	{
		strcpy(fanStr, "AutoFan");
	}
	else if (b1 >= 0x01 && b1 <= 0x07)
	{
		sprintf(fanStr, "Fan%d", b1);
	}
	else
	{
		sprintf(fanStr, "Fan0x%02X", b1);
	}

	// Indoor room temperature: Midea standard (b9 - 30)
	int roomTemp = (int)b9 - 30;

	// Do not print again if nothing changed
	if (changed)
	{
		// Print the full decoded result
		Serial.printf("TW AC: [%s] [%s] [Set:%dC] [%s] [Room:%dC]\r\n"
			, isPowerOn ? "ON" : "OFF"
			, modeStr
			, (int)setTemp
			, fanStr
			, roomTemp
		);
	}
}

void TwoWireTask()
{
	if (p_swSer_TwoWire == nullptr)
	{
		return;
	}

	static unsigned char TwRxBuf[256];
	static unsigned int TwRxLen = 0;
	static unsigned long LastRxMillis = 0;

	while (p_swSer_TwoWire->available() > 0)
	{
		int b = p_swSer_TwoWire->read();
		if (b >= 0)
		{
			if (TwRxLen < sizeof(TwRxBuf))
			{
				TwRxBuf[TwRxLen++] = (unsigned char)b;
			}
			LastRxMillis = millis();
		}
	}

	// Inter-frame timeout: after 10 ms with no new byte, treat the frame as
	// complete and process it
	if (TwRxLen > 0 && (millis() - LastRxMillis) >= 10)
	{
		// Print the raw hex dump
		char PrintBuf[600];
		int PrintLen = sprintf(PrintBuf, "TW UART [%uB]:", TwRxLen);

		for (unsigned int i = 0; i < TwRxLen && PrintLen < 550; i++)
		{
			PrintLen += sprintf(PrintBuf + PrintLen, " %02X", TwRxBuf[i]);
		}
		PrintLen += sprintf(PrintBuf + PrintLen, "\r\n");
		Serial.print(PrintBuf);

		// Parse and print the AC status
		ParseTwoWireAcPacket(TwRxBuf, TwRxLen);

		TwRxLen = 0;
	}
}

void setup()
{
	delay(50);
	Serial.begin(115200);
	Serial.println("TwoWire parser start");

	// AC RX: two-wire bus signal via software serial
	pinMode(MIDEA_AC_RX, INPUT_PULLUP);
	p_swSer_TwoWire = new SoftwareSerial(MIDEA_AC_RX, -1);
	p_swSer_TwoWire->begin(4800);

	Serial.println("Ready");
}

void loop()
{
	TwoWireTask();
}
