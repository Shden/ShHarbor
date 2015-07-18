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

// On button check & handle
int Control::getOnButtonPressed()
{
  int buttonPressed = 0;
  int onReading = digitalRead(CONTROL_ON);
  if (onReading != lastOnReading)
  {
    if (onPressedShort && !onPressedLong)
      // short pressed
      buttonPressed = QUICK_PUSH;
      //promoteFanMode();
      
    onPressedShort = onPressedLong = false;  
    onLastPressTime = millis(); 
  }
   
  if (millis() - onLastPressTime > DEBOUNCE)
  {
    onPressedShort = LOW == onReading;
  } 
    
  if (millis() - onLastPressTime > LONG_PUSH)
  {
    if (!onPressedLong)
    {
      onPressedLong = LOW == onReading;
      if (onPressedLong)
        buttonPressed = LONG_PUSH;
        //pushOnButton(LONG_PUSH);
    }
  }
  lastOnReading = onReading;
  return buttonPressed;
}

int Control::getLightButtonPressed()
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
  Serial.print("Display set to: ");
  Serial.print(mode);
  Serial.println(" value.");
}


