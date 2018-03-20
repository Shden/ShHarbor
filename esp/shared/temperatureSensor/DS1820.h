#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor
{
public:
	// Constructor
	TemperatureSensor(uint8_t pin);

	// To get the latest temperature update from the sensor by index
	float getTemperature(uint8_t sensorIndex);

	// Get 1wire address by index
	void getAddress(uint8_t sensorIndex, char* address);
private:
	OneWire *ow;
	DallasTemperature *sensors;
};

#endif
