#include <DSM501.h>
#include "control.h"
#include "operation.h"
#include "sensor.h"

#define NO__DEBUG__NO

int controllerState;

AsyncDSM501 Sensor;
Control ControlPanel;
Operation OperationModule;

void setup() 
{
  // for debug output
  Serial.begin(9600);

  // Initialize DSM501
  Sensor.begin();

  // Get the current state from operation module as a starting point
  controllerState = OperationModule.getFanState();  
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
  setFanState(FAN_S1);
  delay(2000);

  Serial.println("Setting fan to S2");
  setFanState(FAN_S2);
  delay(2000);

  Serial.println("Setting fan to S3");
  setFanState(FAN_S3);
  delay(2000);

  Serial.println("Turning fan OFF");
  setFanState(FAN_OFF);
  
  return;
  #endif
  /// <--- debug stug code end
  
  // just mirror buttons signal level from the control module to the operation module
  OperationModule.setLightSignal(ControlPanel.getLightButtonSignal());
  OperationModule.setFanSignal(ControlPanel.getOnButtonSignal());
  
  // if operation is OFF, controller does the same
  if (OperationModule.getFanState() == FAN_OFF && controllerState != FAN_AUTO)
  {
    for (int i=0; i<50; i++)
    {
      delay(5);
      if (OperationModule.getFanState() != FAN_OFF)
        return;
    }
    controllerState = FAN_OFF;  
  }
  
  if (controllerState == FAN_S1 || controllerState == FAN_S2 || controllerState == FAN_S3)
    if (OperationModule.getFanState() != controllerState)
    {
      for (int i=0; i<50; i++)
      {
        delay(5);
        if (OperationModule.getFanState() == controllerState)
          return;
      }
      OperationModule.setFanState(controllerState);
    }
    
  // update controller state
  if (ControlPanel.getOnPressed() == QUICK_PUSH)
  {
    // let everything settle
    delay(500);
    
    Serial.print("Changing controller state from: ");
    Serial.print(controllerState);
    switch(controllerState)
    {
      case FAN_OFF:
        Serial.println(" to: S1.");
        controllerState = FAN_S1;
        break;      
      case FAN_S1:
        Serial.println(" to: S2.");
        controllerState = FAN_S2;
        break;      
      case FAN_S2:
        Serial.println(" to: S3.");
        controllerState = FAN_S3;
        break;     
      case FAN_S3:
        Serial.println(" to AUTO mode.");
        controllerState = FAN_AUTO;
        break;
      case FAN_AUTO:
        Serial.println(" now leaving AUTO mode, change speed to S1.");
        controllerState = FAN_S1;
        OperationModule.setFanState(controllerState);
        break;
    }
  }

  // display the current *controller* mode (not the operation module state!)
  ControlPanel.displayMode(controllerState);
  
  // non-blocking particles concentration update
  Sensor.update();
  
  // the fan control itself:
  if (controllerState == FAN_AUTO)
  {
    int AQI = Sensor.getAQI();
         if (AQI > 360) OperationModule.setFanState(FAN_S3);
    else if (AQI > 280) OperationModule.setFanState(FAN_S2);
    else if (AQI > 200) OperationModule.setFanState(FAN_S1);
    else OperationModule.setFanState(FAN_OFF);
  }
}


