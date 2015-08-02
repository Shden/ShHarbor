#include "control.h"

Control::Control()
{
  // initialize control-side pins:
  pinMode(CONTROL_ON, INPUT_PULLUP);
  pinMode(CONTROL_LIGHT, INPUT_PULLUP);
  pinMode(CONTROL_S1_LED, OUTPUT);
  pinMode(CONTROL_S2_LED, OUTPUT);
  pinMode(CONTROL_S3_LED, OUTPUT);
}

// Returns momentary signal from On input
int Control::getOnButtonSignal()
{
  return digitalRead(CONTROL_ON);
}

// Retruns momentart signal from Light input
int Control::getLightButtonSignal()
{
  return digitalRead(CONTROL_LIGHT);
}

/* Updates control module LEDs to display the current mode.
   Possible modes: OFF - all LEDs off
                   S1 - 1st LED on
                   S2 - 2nd LED on
                   S3 - 3rd LED on 
                   AUTO - all LEDs on 
 */
void Control::displayMode(int mode)
{
  digitalWrite(CONTROL_S1_LED, (mode & 0x01) ? HIGH : LOW);
  digitalWrite(CONTROL_S2_LED, (mode & 0x02) ? HIGH : LOW);
  digitalWrite(CONTROL_S3_LED, (mode & 0x04) ? HIGH : LOW);
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


