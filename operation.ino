#include "operation.h"

Operation::Operation()
{
  // initialize operation-side pins:
  pinMode(OPERATION_ON, OUTPUT);
  pinMode(OPERATION_LIGHT, OUTPUT);
  pinMode(OPERATION_S1, INPUT);
  pinMode(OPERATION_S2, INPUT);
  pinMode(OPERATION_S3, INPUT);
}

void Operation::setLight(int value)
{
  digitalWrite(OPERATION_LIGHT, value);
}

// Gets current fan status from operation module
int Operation::getFanSpeed()
{
  int s1 = digitalRead(OPERATION_S1);
  int s2 = digitalRead(OPERATION_S2);
  int s3 = digitalRead(OPERATION_S3); 
  
  if (LOW == s1 && LOW == s2 && LOW == s3)
    return FAN_OFF;
  else if (HIGH == s1)
    return FAN_S1;
  else if (HIGH == s2)
    return FAN_S2;
  else
    return FAN_S3;
}

// Sets fan speed, turns on or off
void Operation::setFanSpeed(int mode)
{
  // Shutting down the fan
  if (mode == FAN_OFF && getFanSpeed() != FAN_OFF)
  {
    Serial.println("Turning fan off...");
    pushOnButton(LONG_PUSH);
    Serial.println("Fan is off now.");
    return;
  }
  
  else if (FAN_S1 == mode || FAN_S2 == mode || FAN_S3 == mode)
  {
    // Check if needed to turn on first
    if (getFanSpeed() == FAN_OFF)
    {
      Serial.println("Turning fan on...");
      pushOnButton(LONG_PUSH);
      Serial.println("Fan is now on.");
      return;
    }
      
    // And adjust the speed cycle
    for (int i=0; i<3; i++)
      if (getFanSpeed() != mode)
        pushOnButton(QUICK_PUSH);
      else break;
      
    Serial.print("Fan speed is set to: ");
    Serial.println(mode);
    return;
  }
}
 
// Push on button for a certain time (ms)
void Operation::pushOnButton(int pushTime)
{
  digitalWrite(OPERATION_ON, LOW);
  delay(pushTime);
  digitalWrite(OPERATION_ON, HIGH);
  Serial.print("On button pushed signal sent for: ");
  Serial.print(pushTime);
  Serial.println(" ms.");
}





