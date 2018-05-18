/*
 * @author Ondrej Pok
 * @version 2.0
 *
 * circuit uses Arduino Nano and its internal pullup resistor.
 * phototransistor is connected between D2 and GND, and second between D3 and GND.
 * 
 * Digital pin is set to HIGH, but when light hits phototransistor, 
 * that pulls the volatage of the pin to the ground so reading of the pin returns LOW.
 * I use interrupts to capture the exact moment the pin is pulled low or goes high.
 *
 * The output on LCD shows shutter speed as well as curtain travel time from sensor to sensor.
 * My sensors are 30x20mm apart, positioned diagonally for measuring horizontal as well as vertical shutter,
 * so the travel time is multiplied by 1.2 to give result for 36mm or 24mm travel.
 * 
 * uses https://github.com/JChristensen/Timer v2 library
 * https://github.com/todbot/SoftI2CMaster for LCD via I2C
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

void printResults(int row, unsigned long exposure, unsigned long travel) {
  lcd.setCursor(0, row);
  lcd.print("                ");
  lcd.setCursor(0, row);
  if(exposure < ONE_SECOND) {
    lcd.print("1/"); lcd.print(round(1.0/((double)exposure/ONE_SECOND)), DEC);
  } else {
    lcd.print(exposure/1000, DEC); lcd.print("ms"); 
  }

  travel = (unsigned long) 1.2 * travel;
  int whole = travel/1000; // whole part from microseconds to miliseconds
  int fraction = (travel - (travel/1000) * 1000)/10; // 2 fractional digits
  lcd.setCursor(10 - floor(log10(whole)), row);
  lcd.print(whole, DEC);
  lcd.print('.');
  if(fraction < 10) lcd.print('0');
  lcd.print(fraction, DEC);
  lcd.print("ms");
}

void onDataReady() {
  Serial.println("onDataReady");
  unsigned long exposure1, exposure2, travel1, travel2;
  exposure1 = s1[1] - s1[0];
  exposure2 = s2[1] - s2[0];
  travel1 = max(s1[0], s2[0]) - min(s1[0], s2[0]);
  travel2 = max(s2[1], s1[1]) - min(s2[1], s1[1]);

  Serial.print("Exposure 1 "); Serial.println(exposure1);
  Serial.print("Exposure 2 "); Serial.println(exposure2);
  Serial.print("Travel 1 "); Serial.println(travel1);
  Serial.print("Travel 2 "); Serial.println(travel2);
  
  printResults(0, exposure1, travel1);
  printResults(1, exposure2, travel2);

  timer.after(1000, resetMeasuredData, (void*)0);
}

void s1_down() {
  if(dataReady) { return; }
  if(s1[0] == 0){
    s1[0] = micros();
  }
  detachInterrupt(digitalPinToInterrupt(SENSOR_1));
  attachInterrupt(digitalPinToInterrupt(SENSOR_1), s1_up, RISING);
  Serial.print("Sensor 1 on at "); Serial.println(s1[0]);
}

void s1_up() {
  if(dataReady) { return; }
  if(s1[1] == 0)
    s1[1] = micros();
  detachInterrupt(digitalPinToInterrupt(SENSOR_1));
  Serial.print("Sensor 1 off at "); Serial.println(s1[1]);
}

void s2_down() {
  if(dataReady) { return; }
  if(s2[0] == 0)
    s2[0] = micros();
  detachInterrupt(digitalPinToInterrupt(SENSOR_2));
  attachInterrupt(digitalPinToInterrupt(SENSOR_2), s2_up, RISING);
  Serial.print("Sensor 2 on at "); Serial.println(s2[0]);
}

void s2_up() {
  if(dataReady) { return; }
  if(s2[1] == 0)
    s2[1] = micros();
  detachInterrupt(digitalPinToInterrupt(SENSOR_2));
  Serial.print("Sensor 2 off at "); Serial.println(s2[1]);
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
