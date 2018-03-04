#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <OneWire.h>

#define CONVERSION_TIME_DS1820		800	// 800 milliseconds for conversion

class TemperatureSensor
{
public:
	// Constructor
	TemperatureSensor(int pin);

	// This will be called from the main loop and do all the stuff
	void updateTemperature();

	// To get the latest update from the sensor
	float getTemperature();
private:
	float lastTemperature = 0.0;
	unsigned long timer = 0;
	byte conversionStarted = 0;
	OneWire *ow;
};

#endif
