#include "Arduino.h"
#include "bathVentilation.h"

#define HIH_PIN				A0
#define RELAY_PIN 			10
#define BVC_BUILD			"0.0.3"
#define DS1820_PIN			12

#define VENTILATION_START_THRESHOLD	75	// Was 65/60 for winter season
#define VENTILATION_STOP_THRESHOLD	70

// Global data used by the contoller. At least keep this in one struct.
struct ControllerData
{
	int ventilationState; 
} GD;


// Controller setup procedure
void setup() 
{
	// Warning: uses global data.
	ControllerData *gd = &GD;
	gd->ventilationState = -1;
	
	Serial.begin(9600);
	Serial.print("Bath ventilation control build: ");
	Serial.println(BVC_BUILD);
	Serial.println("Initialization.");

	pinMode(RELAY_PIN, OUTPUT);
}

// The Loop
void loop()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;
	
	const int hihValue = analogRead(HIH_PIN);
	const float RH = (hihValue - 177) / 6.11;
	Serial.print("Humidity update: ");
	Serial.print(RH);
	Serial.println("%");
	
	// Relay control
	if (RH > VENTILATION_START_THRESHOLD && gd->ventilationState != 1)
	{
		Serial.println("Starting ventilation.");
		digitalWrite(RELAY_PIN, HIGH);
		gd->ventilationState = 1;
	}
	else if (RH < VENTILATION_STOP_THRESHOLD && gd->ventilationState != 0)
	{
		Serial.println("Stopping ventilation.");
		digitalWrite(RELAY_PIN, LOW);
		gd->ventilationState = 0;
	}
		
	// Wait 3 seconds
	delay(3 * 1000); 
}