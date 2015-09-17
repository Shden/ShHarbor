#include "operation.h"

#ifndef CONTROL_H
#define CONTROL_H

// these pins go to the control panel of the hood:
#define CONTROL_ON 8
#define CONTROL_LIGHT 7
#define CONTROL_S1_LED 4
#define CONTROL_S2_LED 5
#define CONTROL_S3_LED 6

// Push button times
enum { LONG_PUSH = 2000, QUICK_PUSH = 100 };

class Control {
public:
  Control();
  int getOnButtonSignal();
  int getLightButtonSignal();
  void displayMode(ControllerStatus mode);
  int getOnPressed();

private:
  long lastOnPush = 0;
  int lastOnState;
};

#endif
