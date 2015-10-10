/*

This is an Arduino Nano based controller for my [IKEA home hood](http://st.houzz.com/simgs/f581a4780d62e398_4-4606/contemporary-range-hoods-and-vents.jpg).

The idea is to use DSM501 dust particles sensor to constantly monitor the air quality in the kitchen.
Then, based on the sensor data the Arduino Nano controller will turn the hood fan to an appropriate
speed until the air got clean and fresh.

An idea of the code for DSM501 is from: https://github.com/richardhmm/DIYRepo/tree/master/arduino/libraries.

Schematics: https://github.com/Shden/ShHarbor/blob/master/docs/schematics.pdf.

*/
#include "control.h"
#include "operation.h"
#include "Arduino.h"
#include "temperatureSensor.h"

// these pins go to the particle density sensor:
#define PARTICLES_10 		2
#define PARTICLES_25 		3
#define DS1820_PIN		A0
#define SPAN_TIME 		60000.0 	// 60 sec time span to measure air quality
#define HC_BUILD 		"1.1.0"
#define TEMP_THRESHOLD_FAN_ON	27.0		// fan is on if above 27.0 degree celcius

#define NO__DEBUG__NO

// Global data used by the controller. At least keep this in one struct.
struct ControllerData
{
	ControllerStatus controllerState;	// Keeps the current state of the controller.
	Control controlPanel;			// This is the front panel representation.
	Operation operationModule;		// This is under the hood operation module representation.
	TemperatureSensor* temperatureSensor;	// DS1820 stuff is hidden here.

	volatile uint32_t tn[2];		// These weird stuff is for dust counting.
	volatile uint32_t tlow[2];
	volatile float lowRatio[2];
	volatile uint32_t spanStart;
} GD;

/*
 * with data sheet...regression function is
 *    y=0.1776*x^3-2.24*x^2+ 94.003*x
 */
float getParticalWeight(int i) 
{	
	// Warning: uses global data.
	ControllerData *gd = &GD;
	
	float r = gd->lowRatio[i];
	float weight = 0.1776 * pow(r, 3) - 0.24 * pow(r, 2) + 94.003 * r;
	return weight < 0.0 ? 0.0 : weight;
}

float getPM25() { return getParticalWeight(0) - getParticalWeight(1); }

// China pm2.5 Index
uint32_t getAQI() 
{
	// this works only under both pin configure
	uint32_t aqi = 0;

	float P25Weight = getPM25();

	if (P25Weight >= 0 && P25Weight <= 35) {
		aqi = 0 + (50.0 / 35 * P25Weight);
	} else if (P25Weight > 35 && P25Weight <= 75) {
		aqi = 50 + (50.0 / 40 * (P25Weight - 35));
	} else if (P25Weight > 75 && P25Weight <= 115) {
		aqi = 100 + (50.0 / 40 * (P25Weight - 75));
	} else if (P25Weight > 115 && P25Weight <= 150) {
		aqi = 150 + (50.0 / 35 * (P25Weight - 115));
	} else if (P25Weight > 150 && P25Weight <= 250) {
		aqi = 200 + (100.0 / 100.0 * (P25Weight - 150));
	} else if (P25Weight > 250 && P25Weight <= 500) {
		aqi = 300 + (200.0 / 250.0 * (P25Weight - 250));
	} else if (P25Weight > 500.0) {
		aqi = 500 + (500.0 / 500.0 * (P25Weight - 500.0)); // Extension
	} else {
		aqi = 0; // Initializing
	}

	return aqi;
}

/* 
	Interrupt handler for both particles channels.

	idx: channel number as follows: 0 for PARTICLES_10, 1 for PARTICLES_25 
	signal: current pin signal value
*/
void particlesHandler(int idx, int signal) 
{
	// Warning: uses global data.
	ControllerData *gd = &GD;
	
	if (!signal) 
	{
		// signal changed to 0: keep time
		gd->tn[idx] = micros();
	} 
	else 
	{
		// signal changed to 1: keep time it was 0
		gd->tlow[idx] += (micros() - gd->tn[idx]);
	}

	if (millis() - gd->spanStart > SPAN_TIME) 
	{

		// tlow in microseconds so div 1000 mul 100 = div 10
		gd->lowRatio[0] = gd->tlow[0] / SPAN_TIME / 10.0;
		gd->lowRatio[1] = gd->tlow[1] / SPAN_TIME / 10.0;

		gd->spanStart = millis();
		gd->tlow[0] = gd->tlow[1] = 0;

		Serial.print("AQI update: ");
		Serial.print(getAQI());
		Serial.print("\tw(0): ");
		Serial.print(getParticalWeight(0));
		Serial.print("\tw(1): ");
		Serial.println(getParticalWeight(1));
		
		Serial.print("Temperature: ");
		Serial.println(gd->temperatureSensor->getTemperature());
		
	}
}

void p10handler() { particlesHandler(0, digitalRead(PARTICLES_10)); }

void p25handler() { particlesHandler(1, digitalRead(PARTICLES_25)); }

/*
	Controller setup procedure.
*/
void setup() 
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	// for debug output
	Serial.begin(9600);
	Serial.print("Hood control build: ");
	Serial.println(HC_BUILD);
	Serial.println("Initialization.");
	
	// Initialize DS1820 wrapper
	gd->temperatureSensor = new TemperatureSensor(DS1820_PIN);

	// Initialize DSM501 pins & attach interrupt handlers
	pinMode(PARTICLES_10, INPUT);
	pinMode(PARTICLES_25, INPUT);
	gd->spanStart = millis();
	gd->lowRatio[0] = gd->lowRatio[1] = 0.0;
	gd->tlow[0] = gd->tlow[1] = 0;
	attachInterrupt(digitalPinToInterrupt(PARTICLES_10), p10handler, CHANGE);
	attachInterrupt(digitalPinToInterrupt(PARTICLES_25), p25handler, CHANGE);

	// Get the current controller state from operation module as a starting point
	gd->controllerState.speed = gd->operationModule.getFanState();
	gd->controllerState.autoMode = 1;
}

// Temperature adjustment for speed. The idea is if temperature is above some threshold value,
// fan should start at S1 regardless to the air quality. This is a 'private' method to use from 
// speedSelect(...) only.
int tempAdjust(int speed, float temperature)
{
	if (temperature > TEMP_THRESHOLD_FAN_ON && FAN_OFF == speed)
		return FAN_S1;
	else
		return speed;
}

// Select the fan speed based on air quality and temperature with histeresis.
//	@currentAQI - current air quialily from DSM501
//	@currentTemp - temperature from DS1820
//	@currentSpeed - how ventilation is working now
int speedSelect(int currentAQI, float currentTemp, int currentSpeed) 
{
	const int AQB[] = { 500, 1000, 2800 };
	const int H = 100;

	// 0 AQI is not valid
	if (currentAQI > 0)
	{
		switch (currentSpeed) 
		{
			case FAN_OFF:
				if (currentAQI > AQB[0] + H)
					return tempAdjust(FAN_S1, currentTemp);
				break;

			case FAN_S1:
				if (currentAQI > AQB[1] + H)
					return tempAdjust(FAN_S2, currentTemp);
				if (currentAQI < AQB[0] - H)
					return tempAdjust(FAN_OFF, currentTemp);
				break;

			case FAN_S2:
				if (currentAQI > AQB[2] + H)
					return tempAdjust(FAN_S3, currentTemp);
				if (currentAQI < AQB[1] - H)
					return tempAdjust(FAN_S1, currentTemp);
				break;
			case FAN_S3:
				if (currentAQI < AQB[2] - H)
					return tempAdjust(FAN_S2, currentTemp);
				break;
		}
	}
	return tempAdjust(currentSpeed, currentTemp);
}

// The main loop.
void loop() 
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	/// debug stub code begin --->
	#ifdef __DEBUG__

	int button = gd->controlPanel.getOnButtonSignal();
	if (button)
		Serial.print(button);

	Serial.println("Setting fan to S1");
	gd->operationModule.setFanState(FAN_S1);
	delay(2000);

	Serial.println("Setting fan to S2");
	gd->operationModule.setFanState(FAN_S2);
	delay(2000);

	Serial.println("Setting fan to S3");
	gd->operationModule.setFanState(FAN_S3);
	delay(2000);

	Serial.println("Turning fan OFF");
	gd->operationModule.setFanState(FAN_OFF);

	return;
	#endif
	/// <--- debug stub code end

	// just mirror buttons signal level from the control module to the operation
	// module
	gd->operationModule.setLightSignal(gd->controlPanel.getLightButtonSignal());
	gd->operationModule.setFanSignal(gd->controlPanel.getOnButtonSignal());

	// if operation is OFF, controller does the same
	if (gd->operationModule.getFanState() == FAN_OFF && !gd->controllerState.autoMode) 
	{
	    /*for (int i=0; i<50; i++)
	    {
	      delay(5);
	      if (operationModule.getFanState() != FAN_OFF)
	        return;
	    }*/
	    gd->controllerState.speed = FAN_OFF;
	}

	if (gd->controllerState.speed)
		if (gd->operationModule.getFanState() != gd->controllerState.speed) 
		{
			for (int i = 0; i < 50; i++) 
			{
				delay(5);
				if (gd->operationModule.getFanState() == gd->controllerState.speed)
				return;
			}
			gd->operationModule.setFanState(gd->controllerState.speed);
		}

	// update controller state
	if (gd->controlPanel.getOnPressed() == QUICK_PUSH) 
	{
		// let everything settle
		delay(500);

		Serial.print("Changing controller state from: ");
		Serial.print(gd->controllerState.speed);

		if (!gd->controllerState.autoMode) 
		{
			// ...in the manual mode:
			switch (gd->controllerState.speed) 
			{
				case FAN_OFF:
					Serial.println(" to: S1.");
					gd->controllerState.speed = FAN_S1;
					break;
				case FAN_S1:
					Serial.println(" to: S2.");
					gd->controllerState.speed = FAN_S2;
					break;
				case FAN_S2:
					Serial.println(" to: S3.");
					gd->controllerState.speed = FAN_S3;
					break;
				case FAN_S3:
					Serial.println(" to AUTO mode.");
					gd->controllerState.autoMode = 1;
					break;
			}
		} 
		else 
		{
			// ... in the auto mode and ON is already released
			Serial.println(" now leaving AUTO mode, change speed to S1.");
			gd->controllerState.autoMode = 0;
			gd->controllerState.speed = FAN_S1;
			gd->operationModule.setFanState(gd->controllerState.speed);
		}
	}

	// display the current *controller* mode (not the operation module state!)
	gd->controlPanel.displayMode(gd->controllerState);
	
	// do the housekeeping for DS1820
	gd->temperatureSensor->updateTemperature();

	// the fan control itself:
	if (gd->controllerState.autoMode) 
	{
		gd->controllerState.speed = speedSelect(getAQI(), gd->temperatureSensor->getTemperature(), gd->controllerState.speed);
		gd->operationModule.setFanState(gd->controllerState.speed);
	}
}
