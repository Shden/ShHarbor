#include <DSM501.h>
#include "control.h"
#include "operation.h"
#include "sensor.h"

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
  switch(ControlPanel.getOnButtonPressed())
  {
    case QUICK_PUSH:
          promoteFanMode();
          break;
    case LONG_PUSH:
          OperationModule.pushOnButton(LONG_PUSH);
          break;
  }
  
  // just mirror the signal from the control module to the operation module
  OperationModule.setLight(ControlPanel.getLightButtonPressed());
  
  // non-blocking particles concentration update
  Sensor.update();
  
  // the fan control itself:
  if (FAN_AUTO == fanControlMode)
  {
         if (Sensor.getAQI() > 240) setFanSpeed(FAN_S3);
    else if (Sensor.getAQI() > 160) setFanSpeed(FAN_S2);
    else if (Sensor.getAQI() > 80) setFanSpeed(FAN_S1);
    else setFanSpeed(FAN_OFF);
  }
}

// Cyclic promotion of the fan mode S1 -> S2 -> S3 -> AUTO -> S1 etc.
void promoteFanMode()
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
}

// Sets fan speed, turns on or off + updates display
void setFanSpeed(int mode)
{
  OperationModule.setFanSpeed(mode);
  ControlPanel.displayMode(mode);
}


