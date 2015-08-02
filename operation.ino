#include "operation.h"

Operation::Operation()
{
  // initialize operation-side pins:
  pinMode(OPERATION_ON, OUTPUT);
  pinMode(OPERATION_LIGHT, OUTPUT);
  pinMode(OPERATION_S1, INPUT);
  pinMode(OPERATION_S2, INPUT);
  pinMode(OPERATION_S3, INPUT);
  digitalWrite(OPERATION_LIGHT, HIGH);
  digitalWrite(OPERATION_ON, HIGH);
}

void Operation::setLightSignal(int value)
{
  digitalWrite(OPERATION_LIGHT, value);
}

void Operation::setFanSignal(int value)
{
  digitalWrite(OPERATION_ON, value);
}

// Gets current fan status from operation module
int Operation::getFanState()
{
  int s1 = digitalRead(OPERATION_S1);
  int s2 = digitalRead(OPERATION_S2);
  int s3 = digitalRead(OPERATION_S3); 
  
  int fanSpeed = -1;
  
  if (LOW == s1 && LOW == s2 && LOW == s3)
    fanSpeed = FAN_OFF;
  else if (HIGH == s1)
    fanSpeed = FAN_S1;
  else if (HIGH == s2)
    fanSpeed = FAN_S2;
  else
    fanSpeed = FAN_S3;
  
  #ifdef __DEBUG__  
  Serial.print("getFanSpeed() returned ");
  Serial.println(fanSpeed);
  #endif
  
  return fanSpeed;
}

// Sets fan speed, turns on or off
void Operation::setFanState(int mode)
{
  // Shutting down the fan
  if (mode == FAN_OFF && getFanState() != FAN_OFF)
  {
    Serial.println("Turning fan off...");
    pushOnButton(LONG_PUSH);
    Serial.println("Fan is off now.");
    return;
  }
  
  else if (FAN_S1 == mode || FAN_S2 == mode || FAN_S3 == mode)
  {
    // Adjust the speed cycle
    for (int i=0; i<3; i++)
      if (getFanState() != mode)
      {
        pushOnButton(200);
        delay(200);
      }
      else break;
      
    //Serial.print("Fan speed now: ");
    //Serial.println(getFanState());
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





