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

// get temperature from single sensor
float TemperatureSensor::getTemperature(uint8_t sensorIndex)
{
	sensors->requestTemperaturesByIndex(sensorIndex);

	float temp = sensors->getTempCByIndex(sensorIndex);
	uint8_t attemptCount = 1;

	while (temp == DEVICE_DISCONNECTED_C && attemptCount++ < 5)
		temp = sensors->getTempCByIndex(sensorIndex);

	return temp;
}

// Get 1wire address by index
void TemperatureSensor::getAddress(uint8_t sensorIndex, char* address)
{
	DeviceAddress sensorAddress;

	if (sensors->getAddress(sensorAddress, sensorIndex))
	{
		const char *hex = "0123456789ABCDEF";
		uint8_t i, j;

		for (i=0, j=0; i<8; i++)
		{
			address[j++] = hex[sensorAddress[i] / 16];
			address[j++] = hex[sensorAddress[i] & 15];
		}
		address[j] = '\0';
	};
}
