#include "Arduino.h"

#ifndef DSM501_H
#define DSM501_H

// these pins go to the particle density sensor:
#define PARTICLES_10 		2
#define PARTICLES_25 		3

#define SPAN_TIME 		60000.0 	// 60 sec time span to measure air quality

class DSM501
{
private:
	volatile uint32_t tn[2];		// These weird stuff is for dust counting.
	volatile uint32_t tlow[2];
	volatile float lowRatio[2];
	volatile uint32_t spanStart;
	
	float getParticalWeight(int i);
public:
	DSM501();
	float getPM25();
	uint32_t getAQI();
	void particlesHandler(int idx, int signal);	
};

#endif