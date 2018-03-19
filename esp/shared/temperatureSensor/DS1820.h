#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor
{
public:
	// Constructor
	TemperatureSensor(int pin);

	// To get the latest update from the sensor
	float getTemperature();
private:
	OneWire *ow;
	DallasTemperature *sensors;
};

#endif
