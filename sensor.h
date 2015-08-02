
#ifndef SENSOR_H
#define SENSOR_H

// these pins go to the particle density sensor:
#define PARTICLES_10  2
#define PARTICLES_25  3

// A non-blocking version of DSM501 class
class AsyncDSM501 : public DSM501
{
  public:
    AsyncDSM501();
    void begin();
    void update();
    //float getParticalWeight(int i);
  private:
    uint32_t _timer;
};

#endif
