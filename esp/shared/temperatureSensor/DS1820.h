#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor
{
public:
	// Constructor
	TemperatureSensor(uint8_t pin);

	// Get the latest temperature from sensor by index
	float getTemperature(int sensorIndex);

	// Get the latest temperature from sensor by address
	float getTemperature(DeviceAddress address);

	// Get 1wire char* address by index
	void getAddress(int sensorIndex, char* address);

	// Get 1wire device addres by index
	bool getAddress(int sensorIndex, DeviceAddress address);

	// Convert DeviceAddress to character string
	void deviceAddresToString(DeviceAddress oneWireAddress, char* address);

	// Convert character string to DeviceAddress
	void stringToDeviceAddress(char* address, DeviceAddress oneWireAddress);
private:
	OneWire *ow;
	DallasTemperature *sensors;
	int char2int(char input);
};

#endif
