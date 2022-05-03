// general libraries
#include <SPI.h>


//display
#include <U8g2lib.h>
U8G2_ST7565_EA_DOGM128_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 17, /* dc=*/ 21, /* reset=*/ 20);
char displayTextBuffer[64];

//DHT22
#include "DHT.h"
#define DHT_TYPE  DHT22
#define DHT_POWER (0)
#define DHT_PIN   (1)
DHT tempSensor(DHT_PIN, DHT_TYPE);
volatile float heater_humidity;
volatile float heater_temperature;
volatile bool heater_loop_heartbeat = false;
#define SENSOR_TASK_TIMER (1000)
void sensorReadCallback();


// temperature control
#define HEATER_1  (3)
#define TEMP_LOW_HISTEREZIS  ((float)0.0)
#define TEMP_HIGH_HISTEREZIS  ((float)0.1)
#define TEMP_COLD_LIMIT       ((float)38)
#define TEMP_WARM_LIMIT       ((float)45)
#define HEATER_MAX_TEMP  ((float)50)
#define LOW_TEMP_ALARM_LIMIT ((float)
volatile float tempSetPoint = 42;
#define CONTROL_TASK_TIMER  (1000)  //it's a slow process, so no hurry
void tempControlCallback();


// motor control
#define MOTOR_1   (4)
volatile uint16_t motorTickInterval = 60;
volatile uint16_t motorTickDuration = 2;
#define MOTOR_TASK_TIMER  (1000)  // control motor in slow intervals
void motorControlCallback();


// interface encoder button
#define BUTTON_UP     (5)
#define BUTTON_DOWN   (6)
#define BUTTON_ENA    (7)
#define UI_TASK_ENCODER (100)
void uiEncoderCallback();


// user interface globals
volatile boolean uiHeaterEnabled = false;
volatile boolean uiHeaterBlink = false;
volatile boolean uiHeaterOverTemp = false;
volatile boolean uiHeaterUnderTemp = false;
volatile boolean uiHeaterError = false;
#define HEATER_DEBUG


boolean ledStatus = true;

//************************* CODE STARTS HERE *********************
void setup() {
  // put your setup code here, to run once:
  Serial.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("BOOT!!!");


  // configure heater and motor pins
  pinMode(HEATER_1, OUTPUT);
  pinMode(MOTOR_1, OUTPUT);
  digitalWrite(HEATER_1, LOW);
  digitalWrite(MOTOR_1, LOW);

  // configure DHT pins
  pinMode(DHT_PIN, INPUT);
  digitalWrite(DHT_PIN, HIGH);
  tempSensor.begin();

  // configure encode button
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_ENA, INPUT_PULLUP);

  // start the display
  //SPI.setRX(16);
  //SPI.setCS(17);
  SPI.setSCK(18);
  SPI.setTX(19);
  SPI.begin(false);

  u8g2.begin();
  // use a fixed font and a fixed contrast
  u8g2.setContrast(100);
  u8g2.setFontDirection(0);
  // start task sched.
}



void loop() {
  // task sched
  heater_temperature = tempSensor.readTemperature();
  heater_humidity = tempSensor.readHumidity();

  u8g2.clearBuffer();
  u8g2.setFontMode(0);
  u8g2.setDrawColor(1);
  
  // draw the current temperature and humidity
  u8g2.setFont(u8g2_font_lubB14_tf);
  sprintf(displayTextBuffer,"temp: %02.1f%cC", heater_temperature, 0xb0);
  u8g2.drawStr(2,16,displayTextBuffer);  // write something to the internal memory
  sprintf(displayTextBuffer,"umid: %02.1f%%",heater_humidity);
  u8g2.drawStr(2,32,displayTextBuffer);  // write something to the internal memory
  u8g2.drawHLine(0,34,128);
  u8g2.drawHLine(0,35,128);

  // graphic symbols to display status
  u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
  if(ledStatus) {
    u8g2.drawGlyph(0, 52, 64); // battery
  } else {
    u8g2.drawGlyph(0, 52, 67); // plugged in
  }
  if(ledStatus) {
    u8g2.drawGlyph(16, 52, 71); // warning
  } else {
    u8g2.drawGlyph(16, 52, 0); // normal
  }
  u8g2.drawGlyph(32, 52, 66); // motor ON
  u8g2.drawGlyph(48, 52, 72); // CONFIG
  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
  u8g2.drawGlyph(64, 52, 69); // heater ON
  
  // temperature and motor control
  u8g2.setFont(u8g2_font_6x12_tf);
  sprintf(displayTextBuffer,"Tset:%02.1f%cC", tempSetPoint, 0xb0);
  u8g2.drawStr(2, 63, displayTextBuffer);


  u8g2.setDrawColor(1);
  u8g2.setFontMode(1);
  u8g2.drawBox(66,54,64,13);
  u8g2.setDrawColor(0);
  sprintf(displayTextBuffer,"Mset:1/100");
  u8g2.drawStr(67, 63, displayTextBuffer);
  u8g2.sendBuffer();
  
  Serial.print("Temp:");
  Serial.println(heater_temperature, 1);
  Serial.print(",Humidity:");
  Serial.println(heater_humidity, 1);
  Serial.println();

  //tempControlCallback();
  //uiDisplayCurrentTempCallback();
  if (true == ledStatus) {
    digitalWrite(LED_BUILTIN, HIGH);
    ledStatus = false;
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    ledStatus = true;
  }
  delay(1000);
}

//update sensor data
void sensorReadCallback() {
  heater_temperature = tempSensor.readTemperature();
  heater_humidity = tempSensor.readHumidity();
}


// control heater elements
void tempControlCallback() {
  // in case of error freeze everything and start buzzer
  uiHeaterError = false;
  heater_loop_heartbeat = (heater_loop_heartbeat) ? false : true;
  if (isnan(heater_temperature)) { // unable to work if env. temperature is faulty
    uiHeaterError = true;
  } else {
    if (!isnan(heater_temperature) && (HEATER_MAX_TEMP <= heater_temperature)) { // stop heater if heater_temperature > 50 and if tere's an usable heater temperature sensor
      digitalWrite(HEATER_1, LOW);
    } else {
      // start heater if temp < (TSET-HISTEREZIS)
      if ( heater_temperature < (tempSetPoint - TEMP_LOW_HISTEREZIS)) {
        digitalWrite(HEATER_1, HIGH);
      }
      // stop heater if temp > (TSET+HISTEREZIS)
      if (heater_temperature > (tempSetPoint + TEMP_HIGH_HISTEREZIS)) {
        digitalWrite(HEATER_1, LOW);
      }

      uiHeaterOverTemp = (heater_temperature > (tempSetPoint + TEMP_HIGH_HISTEREZIS));
      uiHeaterUnderTemp = (heater_temperature < (tempSetPoint - TEMP_LOW_HISTEREZIS));
    }
  }

  // handle error cases
  if (true == uiHeaterError) {
    digitalWrite(HEATER_1, LOW);
  }
  uiHeaterEnabled = digitalRead(HEATER_1) ? true : false;

#ifdef HEATER_DEBUG
  Serial.print("Temp:");
  Serial.print(heater_temperature, 1);
  Serial.print(",Heater:");
  Serial.print(uiHeaterEnabled);
  Serial.print(",Humidity:");
  Serial.println(heater_humidity, 1);
#endif
}

void motorControlCallback() {
  // empty for now
}

void uiDisplayCallback() {
  //displayMeasuredData();
  //displayInterfaceData();
  //displayDebugData();
}
/*
void uiDisplayCurrentTempCallback() {
  uint16_t textColor;
  uint16_t textBackGround;
  textBackGround = ST77XX_BLACK;
  if (uiHeaterError) {
    textColor = ST77XX_RED;
  } else {
    if (uiHeaterOverTemp) {
      textColor = ST77XX_YELLOW;
    } else if (uiHeaterUnderTemp) {
      textColor = ST77XX_BLUE;
    } else {
      textColor = ST77XX_GREEN;
    }
  }

  tft.setCursor(20, 5);
  tft.setTextColor(textColor, textBackGround);
  tft.setTextSize(6);
  if (isnan(heater_temperature)) {
    tft.print("--.-");
  } else {
    tft.print(heater_temperature, 1);
  }
  tft.print(char(0xF7));
  tft.println("C");
}
*/
/*
void uiDisplayCurrentHumidityCallback() {
  uint16_t textColor;
  uint16_t textBackGround;
  textBackGround = ST77XX_BLACK;
  if (uiHeaterError) {
    textColor = ST77XX_RED;
  } else {
    if (uiHeaterOverTemp) {
      textColor = ST77XX_YELLOW;
    } else if (uiHeaterUnderTemp) {
      textColor = ST77XX_BLUE;
    } else {
      textColor = ST77XX_GREEN;
    }
  }
  // display humidity
  tft.setCursor(20, 60);
  if (isnan(heater_humidity)) {
    tft.print("--.-");
  } else {
    tft.print(heater_humidity, 1);
  }
  tft.print("%");
  if (uiHeaterEnabled) {
    tft.println(char(0x07));
  } else {
    if (heater_loop_heartbeat) {
      tft.println(char(0x09));
    } else {
      tft.println(" ");
    }
  }
}
*/

/*
void uiDisplayCurrentSetPointCallback() {
  // and now display the set temperature
  tft.setCursor(10, 120);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(3);
  tft.print("TEMP. DORITA ");
  tft.setCursor(0, 150);
  tft.setTextSize(5);
  tft.print(" ");
  tft.print(tempSetPoint, 1);
  tft.print(char(0xF7));
  tft.println("C ");
}
*/

/*
void uiDisplayDebugDataCallback() {
  // display some debug data
  tft.setCursor(0, 205);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.print("Aux: ");
  if (isnan(heater_temperature)) {
    tft.print("--.-");
  } else {
    tft.print(heater_temperature, 1);
  }
  tft.print(char(0xF7));
  tft.print("C, ");
  if (isnan(heater_humidity)) {
    tft.print("--.-");
  } else {
    tft.print(heater_humidity, 1);
  }
  tft.print("%  ");
  tft.println("");
  tft.println("debug line 2");
}
*/
