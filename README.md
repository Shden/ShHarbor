This is an Arduino Nano based controller for controlling my [IKEA home hood](http://st.houzz.com/simgs/f581a4780d62e398_4-4606/contemporary-range-hoods-and-vents.jpg).

The idea is to use DSM501 dust particles sensor to constantly monitor the air quality in the kitchen.
Then, based on the sensor data the Arduino Nano controller will turn the hood fan to an appropriate
speed until the air got clean and fresh.

A good lib for DSM501 is available: https://github.com/richardhmm/DIYRepo/tree/master/arduino/libraries.

