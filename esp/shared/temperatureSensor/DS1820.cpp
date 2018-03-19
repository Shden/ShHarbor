#include "DS1820.h"
#include <OneWire.h>
#include <DallasTemperature.h>

TemperatureSensor::TemperatureSensor(int pin)
{
	// One wire master will be emulated on this pin:
	ow = new OneWire(pin);

	sensors = new DallasTemperature(ow);
	sensors->begin();
}

float TemperatureSensor::getTemperature()
{
	sensors->requestTemperatures();
	return sensors->getTempCByIndex(0);
}
