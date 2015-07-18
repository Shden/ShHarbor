#ifndef OPERATION_H
#define OPERATION_H

// these pins go to the operation module of the hood:
#define OPERATION_ON      12
#define OPERATION_LIGHT   13
#define OPERATION_S1      9
#define OPERATION_S2      10
#define OPERATION_S3      11

// Possible fan operation states:
enum 
{
  FAN_OFF = 0x00,  // Fan is off
  FAN_S1 = 0x01,   // low speed    
  FAN_S2 = 0x02,   // medium speed
  FAN_S3 = 0x04,   // high speed
  FAN_AUTO = 0x07  // Fan speed depending on air particle density (auto mode)
};

// Possible light states:
/*enum
{
  LIGHT_OFF = 0,    // Light is off
  LIGHT_ON = 1      // Light is on
};*/

class Operation
{
  public:
    Operation();
    void setLight(int value);
    int getFanSpeed();
    void setFanSpeed(int mode);
    void pushOnButton(int pushTime);
};

#endif
