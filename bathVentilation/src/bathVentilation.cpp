/*	Chanagelog:
	20-Aug-2016 - exponential moving average (EMA) algorithm to control on/off thresholds.
*/
#include "Arduino.h"
#include "bathVentilation.h"

#define HIH_PIN				A0
#define RELAY_PIN 			10
#define BVC_BUILD			"0.1.0"
#define DS1820_PIN			12

#define VENTILATION_OFF_THRESHOLD	3
#define VENTILATION_ON_THRESHOLD	5
#define VENTILATION_EMA_STEPS		25 * 60
#define VENTILATION_START_EMA		70

#define VENTILATION_LOOP_TIME		3 * 1000L	// 3 sec in milliseconds

// Global data used by the contoller. At least keep this in one struct.
struct ControllerData
{
	int	ventilationState;
        float	off_threshold;
        float	on_threshold;
        int	EMA_steps;
        float	EMA;
} GD;


// Controller setup procedure
void setup()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;
	gd->ventilationState = -1;
	gd->off_threshold = VENTILATION_OFF_THRESHOLD;
	gd->on_threshold = VENTILATION_ON_THRESHOLD;
	gd->EMA_steps = VENTILATION_EMA_STEPS;
	gd->EMA = VENTILATION_START_EMA;

	Serial.begin(9600);
	Serial.print("Bath ventilation control build: ");
	Serial.println(BVC_BUILD);
	Serial.println("Initialization.");

	pinMode(RELAY_PIN, OUTPUT);
}

// Exponential moving average as follows:
// EMA[k, n] = EMA[k-1, n]+(2/(n+1))·(P-EMA[k-1, n])
float EMA(float n, float previousEMA, float currentValue)
{
	return previousEMA + 2.0/(n + 1.0) * (currentValue - previousEMA);
}

// The Loop
void loop()
{
	// Warning: uses global data.
	ControllerData *gd = &GD;

	const int hihValue = analogRead(HIH_PIN);
	const float RH = (hihValue - 177.0) / 6.11;

	// update EMA humidity in the gd
	gd->EMA = EMA(gd->EMA_steps, gd->EMA, RH);

Serial.print(hihValue);
	Serial.print("Humidity update: ");
	Serial.print(RH);
	Serial.print("%, EMA update: ");
	Serial.println(gd->EMA);

	// Relay control
	if (RH > gd->EMA + gd->on_threshold && gd->ventilationState != 1)
	{
		Serial.println("Starting ventilation.");
		digitalWrite(RELAY_PIN, HIGH);
		gd->ventilationState = 1;
	}
	else if (RH < gd->EMA + gd->off_threshold && gd->ventilationState != 0)
	{
		Serial.println("Stopping ventilation.");
		digitalWrite(RELAY_PIN, LOW);
		gd->ventilationState = 0;
	}

	// Wait 3 seconds
	delay(VENTILATION_LOOP_TIME);
}
