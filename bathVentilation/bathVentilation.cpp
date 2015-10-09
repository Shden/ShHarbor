#include "Arduino.h"
#include "bathVentilation.h"

#define HIH_PIN				456
#define RELAY_PIN 			789
#define BVC_BUILD			"0.0.1"

#define VENTILATION_START_THRESHOLD	70
#define VENTILATION_STOP_THRESHOLD	60

// Controller setup procedure
void setup() 
{
	Serial.begin(9600);
	Serial.print("Bath ventilation control build: ");
	Serial.println(BVC_BUILD);
	Serial.println("Initialization.");

	pinMode(RELAY_PIN, OUTPUT);
}

// The Loop
void loop()
{
	const int hihValue = analogRead(HIH_PIN);
	
	// Relative humidity(RH) (These are the values have taken from http://crazyguy.info/?p=8):
	// 0% = about 163
	// 100% = about 795
	// With roughly linear response.
	// 795 - 163 = 632 (points in the sensor's range)
	// 6.32 points = 1% RH
	
	const float RH = (hihValue - 163) / 6.32;
	Serial.print("Humidity update: ");
	Serial.print(RH);
	Serial.println("%");
	
	// Relay control
	if (RH > VENTILATION_START_THRESHOLD)
	{
		Serial.println("Starting ventilation.");
		digitalWrite(RELAY_PIN, HIGH);
	}
	else if (RH < VENTILATION_STOP_THRESHOLD)
	{
		Serial.println("Stopping ventilation.");
		digitalWrite(RELAY_PIN, LOW);
	}
	
	// Wait 30 seconds
	delay(30 * 1000); 
}