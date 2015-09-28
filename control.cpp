#include "control.h"
#include "Arduino.h"

Control::Control() 
{
	// initialize control-side pins:
	pinMode(CONTROL_ON, INPUT_PULLUP);
	pinMode(CONTROL_LIGHT, INPUT_PULLUP);
	pinMode(CONTROL_S1_LED, OUTPUT);
	pinMode(CONTROL_S2_LED, OUTPUT);
	pinMode(CONTROL_S3_LED, OUTPUT);

	lastOnState = HIGH;
}

// Returns momentary signal from On input
int Control::getOnButtonSignal() { return digitalRead(CONTROL_ON); }

// Retruns momentart signal from Light input
int Control::getLightButtonSignal() { return digitalRead(CONTROL_LIGHT); }

/* Updates control module LEDs to display the current mode. */
void Control::displayMode(ControllerStatus mode) 
{
	// Auto mode flash indication
	if (mode.autoMode) 
	{
		int wheel = millis() % 5000; // each 5 seconds
		if (wheel < 600) 
		{
			if (wheel < 200)
				displayMode({ FAN_S1 });
			else if (wheel < 400)
				displayMode({ FAN_S2 });
			else
				displayMode({ FAN_S3 });
			return;
		}
	}

	digitalWrite(CONTROL_S1_LED, LOW);
	digitalWrite(CONTROL_S2_LED, LOW);
	digitalWrite(CONTROL_S3_LED, LOW);
	digitalWrite(CONTROL_S1_LED, (mode.speed & 0x01) ? HIGH : LOW);
	digitalWrite(CONTROL_S2_LED, (mode.speed & 0x02) ? HIGH : LOW);
	digitalWrite(CONTROL_S3_LED, (mode.speed & 0x04) ? HIGH : LOW);
}

// Monitors On pressed by times
int Control::getOnPressed() 
{
	const int onState = getOnButtonSignal();

	// if nothng changed, just return
	if (lastOnState == onState)
		return 0;

	// if button just pressed, remember the time and return
	if (onState == LOW) 
	{
		lastOnPush = millis();
		lastOnState = onState;
		return 0;
	}

	// if button released, check the timing
	if (onState == HIGH) 
	{
		int res = 0;
		if (millis() - lastOnPush > LONG_PUSH)
			res = LONG_PUSH;
		else if (millis() - lastOnPush > QUICK_PUSH)
			res = QUICK_PUSH;

		lastOnPush = millis();
		lastOnState = onState;

		return res;
	}

	return 0;
}
