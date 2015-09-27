#ifndef OPERATION_H
#define OPERATION_H

// these pins go to the operation module of the hood:
#define OPERATION_ON 	12
#define OPERATION_LIGHT 13
#define OPERATION_S1 	9
#define OPERATION_S2 	10
#define OPERATION_S3 	11

// Possible fan operation states:
enum {
  FAN_OFF = 0, // Fan is off
  FAN_S1 = 1,  // low speed (manual)
  FAN_S2 = 2,  // medium speed (manual)
  FAN_S3 = 4   // high speed (manual)
};

// Hood controller status, including speed and auto mode status
struct ControllerStatus {
  unsigned char speed : 3;
  unsigned char autoMode : 1;
};

// Possible light states:
/*enum
{
  LIGHT_OFF = 0,    // Light is off
  LIGHT_ON = 1      // Light is on
};*/

class Operation {
public:
  Operation();
  void setLightSignal(int value);
  void setFanSignal(int value);
  int getFanState();
  void setFanState(int mode);
  void pushOnButton(int pushTime);
};

#endif
