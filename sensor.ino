#include "sensor.h"

// AsyncDSM501 constructor
AsyncDSM501::AsyncDSM501() : DSM501(PARTICLES_10, PARTICLES_25)
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
    
    Serial.print("AQI update: ");
    Serial.print(getParticalWeight(0));
    Serial.print(" ");
    Serial.print(getParticalWeight(1));
    Serial.print(" ");
    Serial.println(getAQI());
    
    // reset the timer for the new lap
    _timer = millis();
  }
  _win_total[0] ++;
  _low_total[0] += !digitalRead(_pin[PM10_IDX]);
  _win_total[1] ++;
  _low_total[1] += !digitalRead(_pin[PM25_IDX]);
}

void AsyncDSM501::begin()
{
  DSM501::begin(MIN_WIN_SPAN*2);
}



