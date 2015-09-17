#include "control.h"
#include "operation.h"
#include "Arduino.h"

// these pins go to the particle density sensor:
#define PARTICLES_10 2
#define PARTICLES_25 3

#define SPAN_TIME 30000 // 30 sec time span to measure air quality

#define NO__DEBUG__NO

ControllerStatus controllerState;

Control ControlPanel;
Operation OperationModule;

volatile uint32_t tn[2];
volatile uint32_t tlow[2];
volatile float lowRatio[2];
volatile uint32_t spanStart;

unsigned int loopCount = 0;

void particlesHandler(int idx, int signal) {
  if (!signal) {
    // signal changed to 0: keep time
    tn[idx] = micros();
  } else {
    // signal changed to 1: keep time it was 0
    tlow[idx] += (micros() - tn[idx]);
  }

  if (millis() - spanStart > SPAN_TIME) {
    // tlow in microseconds so div 1000 mul 100 = div 10
    lowRatio[0] = tlow[0] / 10.0 / SPAN_TIME;
    lowRatio[1] = tlow[1] / 10.0 / SPAN_TIME;

    spanStart = millis();
    tlow[0] = tlow[1] = 0;
  }
}

void p10handler() { particlesHandler(0, digitalRead(PARTICLES_10)); }

void p25handler() { particlesHandler(1, digitalRead(PARTICLES_25)); }

void setup() {
  // for debug output
  Serial.begin(9600);

  // Initialize DSM501 pins & attach interrupt handlers
  pinMode(PARTICLES_10, INPUT);
  pinMode(PARTICLES_25, INPUT);
  spanStart = millis();
  lowRatio[0] = lowRatio[1] = 0.0;
  tlow[0] = tlow[1] = 0;
  attachInterrupt(digitalPinToInterrupt(PARTICLES_10), p10handler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PARTICLES_25), p25handler, CHANGE);

  // Get the current controller state from operation module as a starting point
  controllerState.speed = OperationModule.getFanState();
  controllerState.autoMode = 0;
}

float getParticalWeight(int i) {
  /*
   * with data sheet...regression function is
   *    y=0.1776*x^3-2.24*x^2+ 94.003*x
   */
  float r = lowRatio[i];
  float weight = 0.1776 * pow(r, 3) - 0.24 * pow(r, 2) + 94.003 * r;
  return weight < 0.0 ? 0.0 : weight;
}

float getPM25() { return getParticalWeight(0) - getParticalWeight(1); }

// China pm2.5 Index
uint32_t getAQI() {
  // this works only under both pin configure
  uint32_t aqi = 0;

  float P25Weight = getPM25();

  if (P25Weight >= 0 && P25Weight <= 35) {
    aqi = 0 + (50.0 / 35 * P25Weight);
  } else if (P25Weight > 35 && P25Weight <= 75) {
    aqi = 50 + (50.0 / 40 * (P25Weight - 35));
  } else if (P25Weight > 75 && P25Weight <= 115) {
    aqi = 100 + (50.0 / 40 * (P25Weight - 75));
  } else if (P25Weight > 115 && P25Weight <= 150) {
    aqi = 150 + (50.0 / 35 * (P25Weight - 115));
  } else if (P25Weight > 150 && P25Weight <= 250) {
    aqi = 200 + (100.0 / 100.0 * (P25Weight - 150));
  } else if (P25Weight > 250 && P25Weight <= 500) {
    aqi = 300 + (200.0 / 250.0 * (P25Weight - 250));
  } else if (P25Weight > 500.0) {
    aqi = 500 + (500.0 / 500.0 * (P25Weight - 500.0)); // Extension
  } else {
    aqi = 0; // Initializing
  }

  return aqi;
}

// Select the fan speed based on air quality with histeresis.
int speedSelect(int currentAQI, int currentSpeed) {
  const int AQB[] = {200, 400, 600};
  const int H = 50;

  switch (currentSpeed) {
  case FAN_OFF:
    if (currentAQI > AQB[0] + H)
      return FAN_S1;
    break;
  case FAN_S1:
    if (currentAQI > AQB[1] + H)
      return FAN_S2;
    if (currentAQI < AQB[0] - H)
      return FAN_OFF;
    break;
  case FAN_S2:
    if (currentAQI > AQB[2] + H)
      return FAN_S3;
    if (currentAQI < AQB[1] - H)
      return FAN_S1;
    break;
  case FAN_S3:
    if (currentAQI < AQB[2] - H)
      return FAN_S2;
    break;
  }
  return currentSpeed;
}

// The main loop.
void loop() {
/*
      Serial.print("AQI update: ");
      Serial.print(getParticalWeight(0));
      Serial.print(" ");
      Serial.print(getParticalWeight(1));
      Serial.print(" ");
      Serial.println(getAQI());

      delay(5000);

      return;
*/

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

  // just mirror buttons signal level from the control module to the operation
  // module
  OperationModule.setLightSignal(ControlPanel.getLightButtonSignal());
  OperationModule.setFanSignal(ControlPanel.getOnButtonSignal());

  // if operation is OFF, controller does the same
  if (OperationModule.getFanState() == FAN_OFF && !controllerState.autoMode) {
    /*for (int i=0; i<50; i++)
    {
      delay(5);
      if (OperationModule.getFanState() != FAN_OFF)
        return;
    }*/
    controllerState.speed = FAN_OFF;
  }

  if (controllerState.speed)
    if (OperationModule.getFanState() != controllerState.speed) {
      for (int i = 0; i < 50; i++) {
        delay(5);
        if (OperationModule.getFanState() == controllerState.speed)
          return;
      }
      OperationModule.setFanState(controllerState.speed);
    }

  // update controller state
  if (ControlPanel.getOnPressed() == QUICK_PUSH) {
    // let everything settle
    delay(500);

    Serial.print("Changing controller state from: ");
    Serial.print(controllerState.speed);

    if (!controllerState.autoMode) {
      // ...in the manual mode:
      switch (controllerState.speed) {
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
    } else {
      // ... in the auto mode and ON is already released
      Serial.println(" now leaving AUTO mode, change speed to S1.");
      controllerState.autoMode = 0;
      controllerState.speed = FAN_S1;
      OperationModule.setFanState(controllerState.speed);
    }
  }

  // display the current *controller* mode (not the operation module state!)
  ControlPanel.displayMode(controllerState);

  //
  if (++loopCount > 10000) {
    loopCount = 0;
    Serial.println(getAQI());
  }

  // the fan control itself:
  if (controllerState.autoMode) {
    controllerState.speed = speedSelect(getAQI(), controllerState.speed);
    OperationModule.setFanState(controllerState.speed);
  }
}
