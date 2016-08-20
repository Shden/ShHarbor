#include "Arduino.h"
#include "DSM501.h"

#define LVC_BUILD		"0.0.1"
#define LIGHT_SENSOR_PIN	A0

struct ControllerData
{
	int	lightOn; 
	DSM501	dustSensor;
} GD;

// Interrupt handlers routed to dust sensor
void p10handler() { GD.dustSensor.particlesHandler(0, digitalRead(PARTICLES_10)); }
void p25handler() { GD.dustSensor.particlesHandler(1, digitalRead(PARTICLES_25)); }

void setup()
{
	Serial.begin(9600);
	Serial.print("Lavatory ventilation control build: ");
	Serial.println(LVC_BUILD);
	Serial.println("Initialization.");

	// Initialize DSM501 pins & attach interrupt handlers
	pinMode(PARTICLES_10, INPUT);
	pinMode(PARTICLES_25, INPUT);
	attachInterrupt(digitalPinToInterrupt(PARTICLES_10), p10handler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(PARTICLES_25), p25handler, CHANGE);

	// Warning: uses global data.
	ControllerData *gd = &GD;
	gd->lightOn = -1;
}

void loop()
{
	const int lightMeasure = analogRead(LIGHT_SENSOR_PIN);
	Serial.println(lightMeasure);
	
	// Warning: uses global data.
	ControllerData *gd = &GD;

	if (lightMeasure > 800 && gd->lightOn != 0)
	{
		Serial.println("Light is off.");
		gd->lightOn = 0;
	}
	
	if (lightMeasure < 600 && gd->lightOn != 1)
	{
		Serial.println("Light is on.");
		gd->lightOn = 1;
	}
	
	delay(2000);
}