#include "DSM501.h"

// Constructor
DSM501::DSM501()
{
	spanStart = millis();
	lowRatio[0] = lowRatio[1] = 0.0;
	tlow[0] = tlow[1] = 0;
}

/*
 * with data sheet...regression function is
 *    y=0.1776*x^3-2.24*x^2+ 94.003*x
 */
float DSM501::getParticalWeight(int i) 
{	
	float r = lowRatio[i];
	float weight = 0.1776 * pow(r, 3) - 0.24 * pow(r, 2) + 94.003 * r;
	return weight < 0.0 ? 0.0 : weight;
}

float DSM501::getPM25() { return getParticalWeight(0) - getParticalWeight(1); }

// China pm2.5 Index
uint32_t DSM501::getAQI() 
{
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

/* 
	Interrupt handler for both particles channels.

	idx: channel number as follows: 0 for PARTICLES_10, 1 for PARTICLES_25 
	signal: current pin signal value
*/
void DSM501::particlesHandler(int idx, int signal) 
{
	if (!signal) 
	{
		// signal changed to 0: keep time
		tn[idx] = micros();
	} 
	else 
	{
		// signal changed to 1: keep time it was 0
		tlow[idx] += (micros() - tn[idx]);
	}

	if (millis() - spanStart > SPAN_TIME) 
	{

		// tlow in microseconds so div 1000 mul 100 = div 10
		lowRatio[0] = tlow[0] / SPAN_TIME / 10.0;
		lowRatio[1] = tlow[1] / SPAN_TIME / 10.0;

		spanStart = millis();
		tlow[0] = tlow[1] = 0;

		Serial.print("AQI update: ");
		Serial.print(getAQI());
		Serial.print("\tw(0): ");
		Serial.print(getParticalWeight(0));
		Serial.print("\tw(1): ");
		Serial.println(getParticalWeight(1));
	}
}