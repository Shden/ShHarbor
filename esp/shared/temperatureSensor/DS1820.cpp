#include "DS1820.h"
#include "OneWire.h"

TemperatureSensor::TemperatureSensor(int pin)
{
	// One wire master will be emulated on this pin:
	ow = new OneWire(pin);
}

void TemperatureSensor::updateTemperature()
{
	if (!conversionStarted)
	{
		// Chat with 1820 is not started yet, starting:
		ow->reset();
		ow->skip();		// use the single device, skip address selection
		ow->write(0x44, 1); 	// start conversion, with parasite power on at the end
		conversionStarted = 1;
		timer = millis();
		return;
	}
	else if (CONVERSION_TIME_DS1820 < millis() - timer || millis() < timer)
	{
		// Chat is in progress and conversation timeout is over, read the data:
		ow->reset();
		ow->skip();
		ow->write(0xBE);         // Read Scratchpad

		byte data[12];
		for (int i = 0; i < 9; i++) // we need 9 bytes
		{
			data[i] = ow->read();
//			Serial.print(data[i], HEX);
//			Serial.print(" ");
		}
//		Serial.print(" CRC=");
//		Serial.print(OneWire::crc8(data, 8), HEX);
//		Serial.println();

		// Check CRC
		if (data[8] != OneWire::crc8(data, 8))
		{
			Serial.println("CRC error.");
			conversionStarted = 0;
			return;
		}

		// Convert the data to actual temperature
		// because the result is a 16 bit signed integer, it should
		// be stored to an "int16_t" type, which is always 16 bits
		// even when compiled on a 32 bit processor.
		int16_t raw = (data[1] << 8) | data[0];
		if (0)
		{
			// this seems to be for some legacy stuff - not the one I use
			raw = raw << 3; // 9 bit resolution default
			if (data[7] == 0x10) {
				// "count remain" gives full 12 bit resolution
				raw = (raw & 0xFFF0) + 12 - data[6];
			}
		}
		else
		{
			byte cfg = (data[4] & 0x60);
			// at lower res, the low bits are undefined, so let's zero them
			if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
			else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
			else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
			//// default is 12 bit resolution, 750 ms conversion time
		}
		lastTemperature = (float)raw / 16.0;
//		Serial.print("  Temperature = ");
//		Serial.print(lastTemperature);
//		Serial.println(" Celsius, ");
		conversionStarted = 0;
	}
}

float TemperatureSensor::getTemperature()
{
	return lastTemperature;
}
