/*
 * @author Ondrej Pok
 * @version 2.1
 *
 * Arduino camera focal plane shutter speed tester
 *
 * Circuit uses Arduino Nano 5V and its internal pullup resistor.
 * 2 phototransistors are connected between D2 and GND, and second between D3 and GND.
 * D2 and D3 because hardware interrupts are available on those pins.
 *
 * Digital input pin is set to HIGH, but when light hits phototransistor, 
 * that pulls the volatage of the pin to the ground so reading of the pin returns LOW.
 * I use interrupts to capture the exact moment the pin is pulled low or goes high.
 * In interrupt handler routine only the microsecond time is saved.
 *
 * 1604 a.k.a. 16 x 4 character LCD is connected via I2C by 4 wires: SDA, SCL and power supply 5V and GND.
 *
 * The output on LCD shows exposure time for each sensor as well as curtain travel time from sensor 1 to sensor 2.
 * My sensors (phototransistors) are 30x20mm apart, positioned diagonally for measuring horizontal as well as vertical shutter,
 * so the travel time is multiplied by 1.2 to give result for 36mm or 24mm travel.
 *
 * uses https://github.com/JChristensen/Timer v2 library
 * https://bitbucket.org/fmalpartida/new-liquidcrystal for LCD via I2C
 *
 * To find the address of your I2C LCD, use I2C scanner from https://github.com/todbot/SoftI2CMaster (also part of New Liquid Crystal library)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <LCD.h>
#include <Timer.h>
#include <LiquidCrystal_I2C.h>

#define SENSOR_1 2
#define SENSOR_2 3

#define I2C_ADDR       0x3f
#define BACKLIGHT_PIN  3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

#define ONE_SECOND 1000000

#define HOLE_X_DISTANCE 30 // keep in mind that hole_Y_distance = hole_X_distance * 24 / 36
#define HOLE_DIAMETER 1.0

// I measured the travel using simpler probe connected to PC sound card and Audacity
// and the tester was giving lot shorter times, so this constant is to compensate.
#define TRAVEL_CORR_COEF 1.15

// see computation of exposure1 in onDataReady()
// This is to compensate for the simple fact that the transistor 
// doesn't open as soon as curtain edge crosses edge of hole.
// It takes a little bit of opening of the hole to let through enough light 
// to open the transistor - let's say 20% of hole diameter. Same for closing curtain
// the transistor closes before the closing curtain completely closes the hole 
// when the amount of light drops enough. This again may be 20%. So we only need 
// to subtract 60% of the hole diameter from raw exposure measurement.
#define EXPOSURE_CORR_COEF 0.6

// if you flip the probe, so S2 is exposed before S1 (S2->S1)
// and the average of several measurements significantly differ from the position (S1->S2)
// then one of these values needs to be adjusted
#define S1_CORR_COEF 1.0
#define S2_CORR_COEF 1.0


LiquidCrystal_I2C lcd (I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

// data storage
// 1st item is when sensor was uncovered and light started hitting the phototransistor
// 2nd item is when sensor was covered again
volatile unsigned long s1[2] = {0, 0}, s2[2] = {0, 0};

volatile bool dataReady = false;

Timer timer;


void resetMeasuredData(void *context) {
  Serial.println("resetting data...");
  s1[0] = 0;
  s1[1] = 0;
  s2[0] = 0;
  s2[1] = 0;
  dataReady = false;
  initInterrupts();
}

void printRightAlignedMs(int row, unsigned long microseconds) {
  int whole = microseconds/1000; // whole part from microseconds to miliseconds
  int decimal = (microseconds % 1000) / 10; // 2 decimal digits
  if(whole > 0)
    lcd.setCursor(10 - floor(log10(whole)), row); // how many digits ?
  else
    lcd.setCursor(10, row); // e.g. 0.12ms
  lcd.print(whole, DEC);
  lcd.print('.');
  if(decimal < 10) lcd.print('0');
  lcd.print(decimal, DEC);
  lcd.print("ms");
}

void printExposure(int row, String description, unsigned long exposure) {
  lcd.setCursor(0, row);
  lcd.print("                "); // clear
  lcd.setCursor(0, row);
  lcd.print(description); lcd.print(" ");

  if(exposure < ONE_SECOND) {
    lcd.print("1/"); lcd.print(round(1.0/((double)exposure/ONE_SECOND)), DEC);  
  }
  printRightAlignedMs(row, exposure);
}

void printTravel(int row, String description, unsigned long travel) {
  lcd.setCursor(0, row);
  lcd.print("                ");
  lcd.setCursor(0, row);
  lcd.print(description); lcd.print(" ");

  printRightAlignedMs(row, travel);
}

void onDataReady() {
  Serial.println("onDataReady");
  unsigned long exposure1, exposure2, travel1, travel2, exp1raw, exp2raw, travel1raw, travel2raw;

  travel1raw = max(s1[0], s2[0]) - min(s1[0], s2[0]);
  travel2raw = max(s2[1], s1[1]) - min(s2[1], s1[1]);
  // sensors approx. 20x30mm apart, extrapolate to 24 or 36mm
  travel1 = TRAVEL_CORR_COEF * (36/HOLE_X_DISTANCE) * travel1raw;
  travel2 = TRAVEL_CORR_COEF * (36/HOLE_X_DISTANCE) * travel2raw;

  exp1raw = s1[1] - s1[0];
  exp2raw = s2[1] - s2[0];
  // for measuring exposure the light starts to shine on sensor 
  //  when the opening curtain crosses the left edge of the hole
  // but we measure the end of exposure when the light stops shining on the sensor 
  //  and that is after closing curtain crosses right edge of the hole
  // Therefore to compensate for the size of the sensor hole
  //  we need to subtract the time that takes the closing curtain to travel across the hole
  exposure1 = S1_CORR_COEF * (exp1raw - EXPOSURE_CORR_COEF * HOLE_DIAMETER * travel2 / 36);
  exposure2 = S2_CORR_COEF * (exp2raw - EXPOSURE_CORR_COEF * HOLE_DIAMETER * travel2 / 36);

  Serial.print("Exposure S1 "); Serial.println(exposure1);
  Serial.print("Exposure S2 "); Serial.println(exposure2);
  Serial.print("Travel opening "); Serial.println(travel1);
  Serial.print("Travel closing "); Serial.println(travel2);
  
  printExposure(0, "E1", exposure1);
  printExposure(1, "E2", exposure2);
  printTravel(2, "Open", travel1);
  printTravel(3, "Close", travel2);

  timer.after(1000, resetMeasuredData, (void*)0);
}

void s1_down() {
  unsigned long measuredValue = micros();
  if(dataReady) { return; }
  if(s1[0] == 0){
    s1[0] = measuredValue;
  }
  detachInterrupt(digitalPinToInterrupt(SENSOR_1));
  attachInterrupt(digitalPinToInterrupt(SENSOR_1), s1_up, RISING);

  //timer to stop waiting for more signals and to reset interrupts after ~2 seconds
  timer.after(2000, resetMeasuredData, (void*)0);
}

void s1_up() {
  unsigned long measuredValue = micros();
  if(dataReady) { return; }
  if(s1[1] == 0)
    s1[1] = measuredValue;
  detachInterrupt(digitalPinToInterrupt(SENSOR_1));
}

void s2_down() {
  unsigned long measuredValue = micros();
  if(dataReady) { return; }
  if(s2[0] == 0)
    s2[0] = measuredValue;
  detachInterrupt(digitalPinToInterrupt(SENSOR_2));
  attachInterrupt(digitalPinToInterrupt(SENSOR_2), s2_up, RISING);
}

void s2_up() {
  unsigned long measuredValue = micros();
  if(dataReady) { return; }
  if(s2[1] == 0)
    s2[1] = measuredValue;
  detachInterrupt(digitalPinToInterrupt(SENSOR_2));
}

void initInterrupts() {
  detachInterrupt(digitalPinToInterrupt(SENSOR_1));
  detachInterrupt(digitalPinToInterrupt(SENSOR_2)); 
  EIFR = 0x03; // reset interrupt register
  attachInterrupt(digitalPinToInterrupt(SENSOR_1), s1_down, FALLING);
  attachInterrupt(digitalPinToInterrupt(SENSOR_2), s2_down, FALLING);
}



void setup()
{
  Serial.begin(9600);
  Serial.println("setting up...");
  
  lcd.begin(16,4);
  lcd.setBacklightPin(BACKLIGHT_PIN,NEGATIVE);

  // set input pins HIGH
  digitalWrite(SENSOR_1, HIGH);
  digitalWrite(SENSOR_2, HIGH);
  
  initInterrupts();
}

void loop()
{
  timer.update();
  
  if(!dataReady && s1[0] != 0 && s1[1] != 0 && s2[0] != 0 && s2[1] != 0) {
    dataReady = true;
    onDataReady();
  }
}
