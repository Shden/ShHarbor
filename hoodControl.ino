#include <DSM501.h>
#include "control.h"
#include "operation.h"
#include "sensor.h"

#define NO__DEBUG__NO

OperationStatus controllerState;

Control ControlPanel;
Operation OperationModule;
AsyncDSM501 Sensor;

void setup() 
{
  // for debug output
  Serial.begin(9600);

  // Initialize DSM501
  Sensor.begin();

  // Get the current controller state from operation module as a starting point
  controllerState.speed = OperationModule.getFanState(); 
  controllerState.autoMode = 0; 
}

// The main loop.
void loop() 
{
  /// debug stub code begin --->
  #ifdef __DEBUG__
  
  int button = ControlPanel.getOnButtonSignal();
  if (button)
    Serial.print(button);
  
  /*delay(2000);
  OperationModule.toggleLight();
  Serial.println("Light changed (1).");
  delay(2000);
  OperationModule.toggleLight();
  Serial.println("Light changed (2).");
  delay(2000);*/

  Serial.println("Setting fan to S1");
  OperationModule.setFanState(FAN_S1);
  delay(2000);

  Serial.println("Setting fan to S2");
  OperationModule.setFanState(FAN_S2);
  delay(2000);

  Serial.println("Setting fan to S3");
  OperationModule.setFanState(FAN_S3);
  delay(2000);

  Serial.println("Turning fan OFF");
  OperationModule.setFanState(FAN_OFF);
  
  return;
  #endif
  /// <--- debug stug code end
  
  // just mirror buttons signal level from the control module to the operation module
  Sensor.update();
  OperationModule.setLightSignal(ControlPanel.getLightButtonSignal());
  Sensor.update();
  OperationModule.setFanSignal(ControlPanel.getOnButtonSignal());
  
  // if operation is OFF, controller does the same
  Sensor.update();
  if (OperationModule.getFanState() == FAN_OFF && !controllerState.autoMode)
  {
    for (int i=0; i<50; i++)
    {
      delay(5);
      if (OperationModule.getFanState() != FAN_OFF)
        return;
    }
    controllerState.speed = FAN_OFF;  
  }
  
  Sensor.update();
  if (controllerState.speed)
    if (OperationModule.getFanState() != controllerState.speed)
    {
      for (int i=0; i<50; i++)
      {
        delay(5);
        if (OperationModule.getFanState() == controllerState.speed)
          return;
      }
      OperationModule.setFanState(controllerState.speed);
    }
    
  // update controller state
  Sensor.update();
  if (ControlPanel.getOnPressed() == QUICK_PUSH)
  {
    // let everything settle
    delay(500);
    
    Serial.print("Changing controller state from: ");
    Serial.print(controllerState.speed);
    
    if (!controllerState.autoMode)
    {
      // ...in the manual mode:
      switch (controllerState.speed)
      {
        case FAN_OFF:
          Serial.println(" to: S1.");
          controllerState.speed = FAN_S1;
          break;      
        case FAN_S1:
          Serial.println(" to: S2.");
          controllerState.speed = FAN_S2;
          break;      
      case FAN_S2:
          Serial.println(" to: S3.");
          controllerState.speed = FAN_S3;
          break;     
      case FAN_S3:
          Serial.println(" to AUTO mode.");
          controllerState.autoMode = 1;
          break;
      }
    }
    else 
    {
      // ... in the auto mode and ON is already released
      Serial.println(" now leaving AUTO mode, change speed to S1.");
      controllerState.autoMode = 0;
      controllerState.speed = FAN_S1;
      OperationModule.setFanState(controllerState.speed);
    }
  }

  // display the current *controller* mode (not the operation module state!)
  Sensor.update();
  ControlPanel.displayMode(controllerState);
  
  // non-blocking particles concentration update
  Sensor.update();
  
  // the fan control itself:
  if (controllerState.autoMode)
  {
    controllerState.speed = speedSelect(Sensor.getParticalWeight(1), controllerState.speed);
    OperationModule.setFanState(controllerState.speed);
  }
  Sensor.update();
}

// Select the fan speed based on air quality with histeresis.
int speedSelect(int currentAQI, int currentSpeed)
{
  const int AQB[] = { 200, 400, 600 };
  const int H = 50;
  
  switch (currentSpeed)
  { 
    case FAN_OFF:
      if (currentAQI > AQB[0] + H) return FAN_S1;
      break;
    case FAN_S1:
      if (currentAQI > AQB[1] + H) return FAN_S2;
      if (currentAQI < AQB[0] - H) return FAN_OFF;
      break;
    case FAN_S2:
      if (currentAQI > AQB[2] + H) return FAN_S3;
      if (currentAQI < AQB[1] - H) return FAN_S1;
      break;
    case FAN_S3:
      if (currentAQI < AQB[2] - H) return FAN_S2;
      break;
  }
  return currentSpeed;
}


