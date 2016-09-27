#include "Arduino.h"
#include "DSM501.h"
#include "Timer.h"

#define LVC_BUILD		"0.0.2"
#define LIGHT_SENSOR_PIN	A0
#define RELAY_PIN 		10
#define LIGHT_CHECK_PERIOD	(2 * 1000L)		// every 2 seconds
#define AIR_CHECK_PERIOD	(15 * 1000L)		// every 15 seconds
#define FAN_CHECK_PERIOD	(5 * 1000L)		// every 5 seconds
#define FAN_ON_DELAY		(1 * 60 * 1000L)	// fan is on 1 minute after light is on
#define FAN_OFF_DELAY		(1 * 60 * 1000L)	// fan is off 1 minute after light is off
#define FAN_MAX_ON_TIME		(20 * 60 * 1000L)	// max fan on time 20 minutes
#define AQI_FAN_ON		500

struct ControllerData
{
	int8_t	lightOn; 
	int8_t	lightFanOn;
	int8_t	AQI_FanOn;
	DSM501	dustSensor;
	Timer	timer;
} GD;

// Interrupt handlers routed to dust sensor
void p10handler() { GD.dustSensor.particlesHandler(0, digitalRead(PARTICLES_10)); }
void p25handler() { GD.dustSensor.particlesHandler(1, digitalRead(PARTICLES_25)); }


void lightFanOn()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;
	if (gd->lightFanOn != 1)
	{
		Serial.println("lightFanOn = 1.");
		gd->lightFanOn = 1;		
	}
}

void lightFanOff()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;
	if (gd->lightFanOn != 0)
	{
		Serial.println("lightFanOn = 0.");
		gd->lightFanOn = 0;		
	}
}

void checkLight()
{
	const int lightMeasure = analogRead(LIGHT_SENSOR_PIN);
	Serial.print("Light: ");
	Serial.println(lightMeasure);
	
	// Warning: uses global data.
	ControllerData *gd = &GD;

	if (lightMeasure > 800 && gd->lightOn != 0)
	{
		Serial.println("Light is off.");
		gd->lightOn = 0;
		gd->timer.after(FAN_OFF_DELAY, lightFanOff);
	}
	
	if (lightMeasure < 600 && gd->lightOn != 1)
	{
		Serial.println("Light is on.");
		gd->lightOn = 1;
		gd->timer.after(FAN_ON_DELAY, lightFanOn);
		gd->timer.after(FAN_MAX_ON_TIME, lightFanOff);
	}
}

void checkAir()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	const int AQI = gd->dustSensor.getAQI();
	Serial.print("AQI: ");
	Serial.println(AQI);
	
	if (AQI > AQI_FAN_ON && gd->AQI_FanOn != 1)
	{
		Serial.println("AQI_FanOn = 1.");
		gd->AQI_FanOn = 1;
	}
	if (AQI < AQI_FAN_ON && gd->AQI_FanOn != 0)
	{
		Serial.println("AQI_FanOn = 0.");
		gd->AQI_FanOn = 0;
	}
}

void checkFan()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;
	
	int fanState = (gd->lightFanOn || gd->AQI_FanOn) ? HIGH : LOW;
	if (digitalRead(RELAY_PIN) != fanState)
	{
		Serial.print("Turning fan ");
		Serial.print(fanState == HIGH ? "ON" : "OFF");
		Serial.println(".");

		digitalWrite(RELAY_PIN, fanState);
	}
}

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

	pinMode(RELAY_PIN, OUTPUT);
	
	gd->timer.every(LIGHT_CHECK_PERIOD, checkLight);
	gd->timer.every(AIR_CHECK_PERIOD, checkAir);
	gd->timer.every(FAN_CHECK_PERIOD, checkFan);
}

void loop()
{
	GD.timer.update();
}

