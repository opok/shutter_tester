# shutter_tester
Arduino camera focal plane shutter speed tester

Circuit uses Arduino Nano and its internal pullup resistor.
2 phototransistors are connected between D2 and GND, and second between D3 and GND.
D2 and D3 because hardware interrupts are available on those pins.

Digital input pin is set to HIGH, but when light hits phototransistor, 
that pulls the volatage of the pin to the ground so reading of the pin returns LOW.
I use interrupts to capture the exact moment the pin is pulled low or goes high.
In interrupt handler routine only the microsecond time is saved.

The output on LCD shows shutter speed as well as curtain travel time from sensor to sensor.
My sensors (phototransistors) are 30x20mm apart, positioned diagonally for measuring horizontal as well as vertical shutter,
so the travel time is multiplied by 1.2 to give result for 36mm or 24mm travel.

uses https://github.com/JChristensen/Timer v2 library
https://github.com/todbot/SoftI2CMaster for LCD via I2C
