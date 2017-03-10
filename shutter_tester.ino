/*
 * circuit:
 *  5V -------------+
 *                  |
 *             phototransistor
 *                  |
 * digital pin -----+
 *                  |
 *                 10kOhm
 *                  |
 *                 gnd
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

// input digital pins
#define SENSOR_1 2
#define SENSOR_2 3

#define I2C_ADDR    0x27 // I2C address of LCD
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

#define ONE_SECOND 1000000

/* Turn on backlight for set period of time
 * disable measuring during this time
 * then reenable measuring and disable backlight.
 */
class LcdBacklight {
  private:
    unsigned long startTime;
    unsigned long period;
    LiquidCrystal_I2C* lcd;
    boolean lcdOn;

  public:
    LcdBacklight() {}
    
    void begin(LiquidCrystal_I2C* lcd, unsigned long seconds) {
      Serial.println("init lcd backlight");
      this->period = seconds;
      this->lcd = lcd;
      this->lcd->setBacklight(HIGH);
      delay(500);
      this->lcd->setBacklight(LOW);
      this->lcdOn = false;      
    }
  
    boolean start() {
      if(this->lcdOn == true) {
        return false;
      }
      Serial.println("lcd backlight turn on");
      this->startTime = millis();
      this->lcd->setBacklight(HIGH);
      delay(200);
      this->lcdOn = true;
      return true;
    }
  
    boolean checkTime() {
      if(this->lcdOn == false) {
        return false;
      }
      if(millis() > this->startTime + this->period * 1000) {
        Serial.println("lcd backlight turn off");
        this->lcd->setBacklight(LOW);
        delay(100);
        this->lcdOn = false;
        return true;
      }
      return false;
    }
};


LiquidCrystal_I2C lcd (I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

// sensor data arrays, index 0 is when the value went HIGH, index 1 is when it went LOW
volatile unsigned long s1[2] = {0, 0}, s2[2] = {0, 0};
volatile bool dataReady = false;
LcdBacklight lcdBacklight;

void resetMeasuredData() {
  dataReady = false;
  s1[0] = 0;
  s1[1] = 0;
  s2[0] = 0;
  s2[1] = 0;
}

void printResult(int row, unsigned long exposure, unsigned long travel) {
  lcd.setCursor(0, row);
  lcd.print("                ");
  lcd.setCursor(0, row);
  if(exposure < ONE_SECOND) {
    lcd.print("1/"); lcd.print(round(1.0/((double)exposure/ONE_SECOND)), DEC);
  } else {
    lcd.print(exposure/1000, DEC); lcd.print("ms"); 
  }

  
  int whole = travel/1000; // whole part from microseconds to miliseconds
  int fraction = (travel - (travel/1000) * 1000)/10; // 2 fractional digits
  lcd.setCursor(10 - floor(log10(whole)), row); // log10 -> to tell how many digits the whole part has
  lcd.print(whole, DEC);
  lcd.print('.');
  if(fraction < 10) lcd.print('0'); // leading zero
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

  printResult(0, exposure1, travel1);
  printResult(1, exposure2, travel2);

  lcdBacklight.start();
}

void resetPendingInterrupts() {
  EIFR = 0x03; // reset interrupt register, not sure if this works
}

void s1_up() {
  resetMeasuredData();
  s1[0] = micros();
  Serial.println("s1_up");
  detachInterrupt(digitalPinToInterrupt(SENSOR_1));
  attachInterrupt(digitalPinToInterrupt(SENSOR_1), s1_down, FALLING);
}

void s1_down() {
  s1[1] = micros();
  detachInterrupt(digitalPinToInterrupt(SENSOR_1));
}

void s2_up() {
  s2[0] = micros();
  detachInterrupt(digitalPinToInterrupt(SENSOR_2));
  attachInterrupt(digitalPinToInterrupt(SENSOR_2), s2_down, FALLING);
}

void s2_down() {
  s2[1] = micros();
  Serial.println("s2_down");
  detachInterrupt(digitalPinToInterrupt(SENSOR_2));
  dataReady = true; // after measuring last value, update displayed data on LCD
}

void initInterrupts() {
  Serial.println("initInterrupts");
  resetPendingInterrupts();
  attachInterrupt(digitalPinToInterrupt(SENSOR_1), s1_up, RISING);  
  attachInterrupt(digitalPinToInterrupt(SENSOR_2), s2_up, RISING);
}

void setup()
{
  lcd.begin(16,2);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcdBacklight.begin(&lcd, 10);

  initInterrupts();
  
  Serial.begin(9600);
}

void loop()
{
  if(lcdBacklight.checkTime()) {
    Serial.println("loop");
    initInterrupts();
  }
  if(dataReady == true) {
    onDataReady();
    dataReady = false;
  }
}

