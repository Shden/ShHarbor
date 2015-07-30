#ifndef CONTROL_H
#define CONTROL_H

// these pins go to the control panel of the hood:
#define CONTROL_ON       8
#define CONTROL_LIGHT    7
#define CONTROL_S1_LED   4
#define CONTROL_S2_LED   5
#define CONTROL_S3_LED   6

// Push button times:
enum
{ 
  LONG_PUSH = 2000,
  QUICK_PUSH = 200,
  DEBOUNCE = 25
};

class Control
{
  public:
    Control();
    int getOnButtonPressed();
    int getLightButtonPressed();
    void displayMode(int mode);
  /*private:
    bool onPressedShort = false, onPressedLong = false;
    long onLastPressTime = 0;
    int lastOnReading = HIGH;*/
};

#endif
