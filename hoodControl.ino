#include <DSM501.h>

// these pins go to the operation module of the hood:
const int operationOn = 12;
const int operationLight = 13;
const int operationS1 = 9;
const int operationS2= 10;
const int operationS3 = 11;

// these pins go to the control shield of the hood:
const int controlOn = 7;
const int controlLight = 8;
const int controlS1 = 4;
const int controlS2 = 5;
const int controlS3 = 6;

// these pins go to the particle density sensor:
const int particles10 = 2;
const int particles25 = 3;

// A non-blocking version of DSM501 class
class AsyncDSM501 : public DSM501
{
  public:
    AsyncDSM501(int pin10, int pin25);
    void update();
    float getParticalWeight(int i);
  private:
    uint32_t _timer;
};

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
enum
{
  LIGHT_OFF = 0,    // Light is off
  LIGHT_ON = 1      // Light is on
};

// Push button times:
enum
{ 
  LONG_PUSH = 1000,
  QUICK_PUSH = 250,
  DEBOUNCE = 50
};

AsyncDSM501 dsm501(particles10, particles25);

int fanControlMode = FAN_OFF;

void setup() 
{
  // initialize operation-side pins:
  pinMode(operationOn, OUTPUT);
  pinMode(operationLight,  OUTPUT);
  pinMode(operationS1, INPUT);
  pinMode(operationS2, INPUT);
  pinMode(operationS3, INPUT);

  // initialize control-side pins:
  pinMode(controlOn, INPUT_PULLUP);
  pinMode(controlLight, INPUT_PULLUP);
  pinMode(controlS1, OUTPUT);
  pinMode(controlS2, OUTPUT);
  pinMode(controlS3, OUTPUT);

  // Initialize DSM501
  dsm501.begin(MIN_WIN_SPAN);
  
  fanControlMode = FAN_AUTO;
  
  // for debug output
  Serial.begin(9600);
  Serial.println("Initialization completed.");
}

// The main loop.
void loop() 
{
  onButtonCheckAndHandle();
  lightButtonCheckAndHandle();
  
  // non-blocking particles concentration update
  dsm501.update();
  
  // the fan control itself:
  if (FAN_AUTO == fanControlMode)
  {
         if (dsm501.getAQI() > 240) setFanSpeed(FAN_S3);
    else if (dsm501.getAQI() > 160) setFanSpeed(FAN_S2);
    else if (dsm501.getAQI() > 80) setFanSpeed(FAN_S1);
    else setFanSpeed(FAN_OFF);
  }
}

bool onPressedShort = false, onPressedLong = false;
long onLastPressTime = 0;
int lastOnReading = HIGH;

// On button check & handle
void onButtonCheckAndHandle()
{
  int onReading = digitalRead(controlOn);
  if (onReading != lastOnReading)
  {
    if (onPressedShort && !onPressedLong)
      // short pressed
      promoteFanMode();
      
    onPressedShort = onPressedLong = false;  
    onLastPressTime = millis(); 
  }
   
  if (millis() - onLastPressTime > DEBOUNCE)
  {
    onPressedShort = LOW == onReading;
  } 
    
  if (millis() - onLastPressTime > LONG_PUSH)
  {
    if (!onPressedLong)
    {
      onPressedLong = LOW == onReading;
      if (onPressedLong)
        pushOnButton(LONG_PUSH);
    }
  }
  lastOnReading = onReading;
}

// Light button check & handle
void lightButtonCheckAndHandle()
{
  // just mirror the signal from the control module to the operation module
  digitalWrite(operationLight, digitalRead(controlLight));
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
      displayMode(fanControlMode);
      break;
    case FAN_AUTO:
      fanControlMode = FAN_S1;
      setFanSpeed(fanControlMode);
      break;
  }
}

// Gets current fan status from operation module
int getFanSpeed()
{
  int s1 = digitalRead(operationS1);
  int s2 = digitalRead(operationS2);
  int s3 = digitalRead(operationS3); 
  
  if (LOW == s1 && LOW == s2 && LOW == s3)
    return FAN_OFF;
  else if (HIGH == s1)
    return FAN_S1;
  else if (HIGH == s2)
    return FAN_S2;
  else
    return FAN_S3;
}

// Sets fan speed, turns on or off + updates display
void setFanSpeed(int mode)
{
  // Shutting down the fan
  if (mode == FAN_OFF && getFanSpeed() != FAN_OFF)
  {
    Serial.println("Turning fan off...");
    pushOnButton(LONG_PUSH);
    displayMode(mode);
    Serial.println("Fan is off now.");
    return;
  }
  
  else if (FAN_S1 == mode || FAN_S2 == mode || FAN_S3 == mode)
  {
    // Check if needed to turn on first
    if (getFanSpeed() == FAN_OFF)
    {
      Serial.println("Turning fan on...");
      pushOnButton(LONG_PUSH);
      displayMode(mode);
      Serial.println("Fan is now on.");
      return;
    }
      
    // And adjust the speed cycle
    for (int i=0; i<3; i++)
      if (getFanSpeed() != mode)
        pushOnButton(QUICK_PUSH);
      else break;
      
    displayMode(mode);
    Serial.print("Fan speed is set to: ");
    Serial.println(mode);
    return;
  }
}
 
/* Updates control module LEDs to display the current mode.
   Possible modes: OFF - all LEDs off
                   S1 - 1st LED on
                   S2 - 2nd LED on
                   S3 - 3rd LED on 
                   AUTO - all LEDs on 
 */
void displayMode(int mode)
{
  digitalWrite(operationS1, (mode & 0x01) ? HIGH : LOW);
  digitalWrite(operationS2,  (mode & 0x02) ? HIGH : LOW);
  digitalWrite(operationS3,  (mode & 0x04) ? HIGH : LOW);
  Serial.print("Display set to: ");
  Serial.print(mode);
  Serial.println(" value.");
}

// Push on button for a certain time (ms)
void pushOnButton(int pushTime)
{
  digitalWrite(operationOn, LOW);
  delay(pushTime);
  digitalWrite(operationOn, HIGH);
  Serial.print("On button pushed signal sent for: ");
  Serial.print(pushTime);
  Serial.println(" ms.");
}

// Push light button
/*void pushLightButton()
{
  digitalWrite(operationLight, LOW);
  delay(500);
  digitalWrite(operationLight, HIGH);
  Serial.println("Light button push signal sent.");
}

int getLightMode()
{
  
}

void setLightMode(int mode)
{
}
*/

// AsyncDSM501.
// AsyncDSM501 constructor
AsyncDSM501::AsyncDSM501(int pin10, int pin25) : DSM501(pin10, pin25)
{
  _timer = millis();
}

// Override. Non-blocking update method.
void AsyncDSM501::update()
{
  if (millis() < _timer || millis() - _timer >= _span)
  {
    _done[0] = 1;
    _done[1] = 1;
    
    // promote new low ratio to the parent class variables so that we can call getParticalWeight(i) etc.
    // getLowRatio(i) will reset _win_total[i], _low_total[i] and _done[i]
    getLowRatio(0);
    getLowRatio(1);
    
    // reset the timer for the new lap
    _timer = millis();
  }
  _win_total[0] ++;
  _low_total[0] += !digitalRead(_pin[PM10_IDX]);
  _win_total[1] ++;
  _low_total[1] += !digitalRead(_pin[PM25_IDX]);
}


