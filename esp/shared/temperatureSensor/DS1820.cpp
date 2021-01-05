#include "DS1820.h"
#include <OneWire.h>
#include <DallasTemperature.h>

TemperatureSensor::TemperatureSensor(uint8_t pin)
{
	// One wire master will be emulated on this pin:
	ow = new OneWire(pin);

	sensors = new DallasTemperature(ow);
	sensors->begin();
	sensors->setResolution(10); // 10 bit resolution
}

// get temperature from single sensor by index
float TemperatureSensor::getTemperature(int sensorIndex)
{
	sensors->requestTemperaturesByIndex(sensorIndex);

	float temp = sensors->getTempCByIndex(sensorIndex);
	uint8_t attemptCount = 1;

	while (temp == DEVICE_DISCONNECTED_C && attemptCount++ < 5)
		temp = sensors->getTempCByIndex(sensorIndex);

	return temp;
}

// get temperature from sensor by address
float TemperatureSensor::getTemperature(DeviceAddress address)
{
// Serial.print("Requested getTemperatureByAddress address: ");
// for (int i=0; i<14; i++)
// 	Serial.printf("%c", address[i]);
// Serial.println();

	sensors->requestTemperaturesByAddress(address);

	float temp = sensors->getTempC(address);
	uint8_t attemptCount = 1;

	while (temp == DEVICE_DISCONNECTED_C && attemptCount++ < 5)
		temp = sensors->getTempC(address);

	return temp;
}

void TemperatureSensor::deviceAddresToString(DeviceAddress oneWireAddress, char* address)
{
	const char *hex = "0123456789ABCDEF";
	uint8_t i, j;

	for (i=0, j=0; i<8; i++)
	{
		address[j++] = hex[oneWireAddress[i] / 16];
		address[j++] = hex[oneWireAddress[i] & 15];
	}
	address[j] = '\0';

}

// Get 1wire char* address by index
void TemperatureSensor::getAddress(int sensorIndex, char* address)
{
	DeviceAddress sensorAddress;

	if (getAddress(sensorIndex, sensorAddress))
		deviceAddresToString(sensorAddress, address);
}

// Get 1wire device addres by index
bool TemperatureSensor::getAddress(int sensorIndex, DeviceAddress address)
{
	return sensors->getAddress(address, sensorIndex);
}

int TemperatureSensor::char2int(char input)
{
  if(input >= '0' && input <= '9')
    return input - '0';
  if(input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if(input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  return -1;
}

// Convert character string to DeviceAddress
void TemperatureSensor::stringToDeviceAddress(char* address, DeviceAddress oneWireAddress)
{
	for (int i = 0; i < 8; i++)
	{
		Serial.printf("%c%c-", address[2 * i], address[2 * i + 1]);
		oneWireAddress[i] = char2int(address[2 * i]) << 4 | char2int(address[2 * i + 1]);
		Serial.printf("%X\n\r", oneWireAddress[i]);
	}
}
