#include <DSM501.h>
#include "control.h"
#include "operation.h"
#include "sensor.h"

#define NO__DEBUG__NO

AsyncDSM501 Sensor;
Control ControlPanel;
Operation OperationModule;

int fanControlMode = FAN_OFF;

void setup() 
{
  // for debug output
  Serial.begin(9600);

  // Initialize DSM501
  Sensor.begin();
  
  fanControlMode = FAN_AUTO;
  
  Serial.println("Initialization completed.");
}

// The main loop.
void loop() 
{
  /// debug stub code begin --->
  #ifdef __DEBUG__
  
  int button = ControlPanel.getOnButtonPressed();
  if (button)
    Serial.print(button);
  return;
  
  delay(2000);
  OperationModule.toggleLight();
  Serial.println("Light changed (1).");
  delay(2000);
  OperationModule.toggleLight();
  Serial.println("Light changed (2).");
  delay(2000);

  Serial.println("Setting fan to S1");
  setFanSpeed(FAN_S1);
  delay(2000);

  Serial.println("Setting fan to S2");
  setFanSpeed(FAN_S2);
  delay(2000);

  Serial.println("Setting fan to S3");
  setFanSpeed(FAN_S3);
  delay(2000);

  Serial.println("Turning fan OFF");
  setFanSpeed(FAN_OFF);
  
  return;
  #endif
  /// <--- debug stug code end
  
  /*switch(ControlPanel.getOnButtonPressed())
  {
    case QUICK_PUSH:
          promoteFanMode();
          break;
    case LONG_PUSH:
          OperationModule.pushOnButton(LONG_PUSH);
          ControlPanel.displayMode(OperationModule.getFanSpeed());
          break;
  }*/
  
  // just mirror the light signal from the control module to the operation module
  OperationModule.setLight(ControlPanel.getLightButtonPressed());
  OperationModule.setFan(ControlPanel.getOnButtonPressed());
  
  if (ControlPanel.getOnButtonPressed())
  {
    if (fanControlMode != FAN_AUTO)
    {
      if (FAN_S3 == fanControlMode && FAN_S1 == OperationModule.getFanSpeed())
      {
        Serial.println("Enter auto mode.");
        fanControlMode = FAN_AUTO;
      }
      else
      {
        fanControlMode = OperationModule.getFanSpeed();
      }
    }
    else 
    {
      if (FAN_S1 != OperationModule.getFanSpeed())
      {
        Serial.println("Leave auto mode.");
        fanControlMode = OperationModule.getFanSpeed();
      }
    }
  }
  
  // display current mode
  ControlPanel.displayMode(fanControlMode);
  
  // non-blocking particles concentration update
  Sensor.update();
  
  // the fan control itself:
  if (!ControlPanel.getOnButtonPressed())
  {
  if (FAN_AUTO == fanControlMode)
  {
    int currentFanSpeed = OperationModule.getFanSpeed();
    int AQI = 162;//Sensor.getAQI() * 0; /// debug!!!
         if (AQI > 240 && FAN_S3 != currentFanSpeed) OperationModule.setFanSpeed(FAN_S3);
    else if (AQI > 160 && FAN_S2 != currentFanSpeed) OperationModule.setFanSpeed(FAN_S2);
    else if (AQI > 80  && FAN_S1 != currentFanSpeed) OperationModule.setFanSpeed(FAN_S1);
    else if (FAN_OFF != currentFanSpeed) OperationModule.setFanSpeed(FAN_OFF);
  }
  }
}

// Cyclic promotion of the fan mode S1 -> S2 -> S3 -> AUTO -> S1 etc.
/*void promoteFanMode()
{
  switch(fanControlMode)
  {
    case FAN_S1:
      fanControlMode = FAN_S2;
      setFanSpeed(fanControlMode);
      break;
    case FAN_S2:
      fanControlMode = FAN_S3;
      setFanSpeed(fanControlMode);
      break;
    case FAN_S3:
      fanControlMode = FAN_AUTO;
      ControlPanel.displayMode(fanControlMode);
      break;
    case FAN_AUTO:
      fanControlMode = FAN_S1;
      setFanSpeed(fanControlMode);
      break;
  }
}*/

// Sets fan speed, turns on or off + updates display
/*void setFanSpeed(int mode)
{
  OperationModule.setFanSpeed(mode);
  ControlPanel.displayMode(mode);
}*/


