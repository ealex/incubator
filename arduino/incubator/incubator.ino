 // general libraries
#include <SPI.h>


//display
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#define TFT_CS        -1
#define TFT_RST        8 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         9
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
#define UI_TASK_CURRENT_TEMP (500)
void uiDisplayCurrentTempCallback();
#define UI_TASK_CURRENT_HUMIDITY (500)
void uiDisplayCurrentHumidityCallback();
#define UI_TASK_CURRENT_SETPOINT (100)
void uiDisplayCurrentSetPointCallback();
#define UI_TASK_DEBUG_DATA        (1000)
void uiDisplayDebugDataCallback();


//DHT22
#include "DHT.h"
#define DHT_TYPE  DHT22
#define DHT_POWER   7
#define DHT_1_PIN   6
#define DHT_2_PIN   5
DHT dht_heater(DHT_1_PIN, DHT_TYPE);
DHT dht_env(DHT_2_PIN, DHT_TYPE);
volatile float heater_humidity;
volatile float heater_temperature;
volatile float env_humidity;
volatile float env_temperature;
volatile bool heater_loop_heartbeat = false;
#define SENSOR_TASK_TIMER (1000)
void sensorReadCallback();


// temperature control
#define HEATER_1  (A0)
#define HEATER_2  (A1)
#define TEMP_LOW_HISTEREZIS  ((float)0.0)
#define TEMP_HIGH_HISTEREZIS  ((float)0.1)
#define TEMP_COLD_LIMIT       ((float)37.0)
#define TEMP_WARM_LIMIT       ((float)38.0)
#define HEATER_MAX_TEMP  ((float)50)
#define LOW_TEMP_ALARM_LIMIT ((float)
volatile float tempSetPoint=37.5;
#define CONTROL_TASK_TIMER  (1000)  //it's a slow process, so no hurry
void tempControlCallback();


// motor control
#define MOTOR_1   (A2)
volatile uint16_t motorTickInterval = 60;
volatile uint16_t motorTickDuration = 2;
#define MOTOR_TASK_TIMER  (1000)  // control motor in slow intervals
void motorControlCallback();


// interface encoder button
#define BUTTON_UP     (A3)
#define BUTTON_DOWN   (A4)
#define BUTTON_ENA    (A5)
#define UI_TASK_ENCODER (100)
void uiEncoderCallback();


// user interface globals
volatile boolean uiHeaterEnabled=false;
volatile boolean uiHeaterBlink=false;
volatile boolean uiHeaterOverTemp=false;
volatile boolean uiHeaterUnderTemp=false;
volatile boolean uiHeaterError = false;
//#define HEATER_DEBUG


// task manager
#include <TaskScheduler.h>
Scheduler runner;
// display related tasks
Task uiCurrentTempTask(UI_TASK_CURRENT_TEMP, TASK_FOREVER, &uiDisplayCurrentTempCallback, &runner, true);  //user interface handling task
Task uiCurrentHumidityTask(UI_TASK_CURRENT_HUMIDITY, TASK_FOREVER, &uiDisplayCurrentHumidityCallback, &runner, true);  //user interface handling task
Task uiCurrentSetPointTask(UI_TASK_CURRENT_SETPOINT, TASK_FOREVER, &uiDisplayCurrentSetPointCallback, &runner, true);  //user interface handling task
Task uiDisplayDebugTask(UI_TASK_DEBUG_DATA, TASK_FOREVER, &uiDisplayDebugDataCallback, &runner, true);  //user interface handling task
// hardware interface related tasks
Task sensorTask(SENSOR_TASK_TIMER, TASK_FOREVER, &sensorReadCallback, &runner, true); // read sensors
Task controlTask(CONTROL_TASK_TIMER, TASK_FOREVER, &tempControlCallback, &runner, true); // control heaters and alarms
Task motorTask(MOTOR_TASK_TIMER, TASK_FOREVER, &motorControlCallback, &runner, true); // control egg movement motor


//************************* CODE STARTS HERE *********************
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // configure heater and motor pins
  pinMode(HEATER_1, OUTPUT);
  pinMode(HEATER_2, OUTPUT);
  pinMode(MOTOR_1, OUTPUT);
  digitalWrite(HEATER_1,LOW);  
  digitalWrite(HEATER_2,LOW);
  digitalWrite(MOTOR_1,LOW);

  // configure DHT pins and power both sensors
  pinMode(DHT_POWER,OUTPUT);
  digitalWrite(DHT_POWER,HIGH);
  dht_heater.begin();
  dht_env.begin();

  // configure encode button
  pinMode(BUTTON_UP,INPUT_PULLUP);
  pinMode(BUTTON_DOWN,INPUT_PULLUP);
  pinMode(BUTTON_ENA,INPUT_PULLUP);
  

  // start the display
  // tft.setSPISpeed(1000);
  tft.init(240, 240,SPI_MODE2);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.drawLine(0, 110, 239, 110, ST77XX_MAGENTA);


  // start task sched.
  runner.startNow();
}



void onUiButtonHandler(EncoderButton& eb) {
  Serial.print("eb1 incremented by: ");
  Serial.println(eb.increment());
  Serial.print("eb1 position is: ");
  Serial.println(eb.position());
}

void onUiButtonLongPress(EncoderButton& eb) {
  Serial.print("button1 longPressCount: ");
  Serial.println(eb.longPressCount());
}


void loop() {
  // task sched 
  runner.execute();
}

//update sensor data
void sensorReadCallback() {
  heater_temperature = dht_heater.readTemperature();
  heater_humidity = dht_heater.readHumidity();
  env_humidity = dht_env.readHumidity();
  env_temperature = dht_env.readTemperature();
}


// control heater elements
void tempControlCallback() {
  // in case of error freeze everything and start buzzer
  uiHeaterError = false;
  heater_loop_heartbeat = (heater_loop_heartbeat)? false:true;
  if(isnan(env_temperature)) { // unable to work if env. temperature is faulty
    uiHeaterError=true;
  } else {
    if (!isnan(heater_temperature) && (HEATER_MAX_TEMP <= heater_temperature)) { // stop heater if heater_temperature > 50 and if tere's an usable heater temperature sensor
      digitalWrite(HEATER_1,LOW);
    } else {
      // start heater if temp < (TSET-HISTEREZIS)
      if( env_temperature < (tempSetPoint-TEMP_LOW_HISTEREZIS)) {
        digitalWrite(HEATER_1,HIGH);
      }
      // stop heater if temp > (TSET+HISTEREZIS)
      if(env_temperature > (tempSetPoint+TEMP_HIGH_HISTEREZIS)) {
        digitalWrite(HEATER_1,LOW);
      }
      
      uiHeaterOverTemp = (env_temperature > TEMP_WARM_LIMIT);
      uiHeaterUnderTemp = (env_temperature <TEMP_COLD_LIMIT);
    }
  }

  // handle error cases
  if(true==uiHeaterError) {
    digitalWrite(HEATER_1,LOW);
  }
  uiHeaterEnabled = digitalRead(HEATER_1)?true:false;  
  
#ifdef HEATER_DEBUG
  Serial.print("Temp:");  
  Serial.print(env_temperature,1);
  Serial.print(",Heater:");
  Serial.print(uiHeaterEnabled);
  Serial.print(",Humidity:");
  Serial.println(env_humidity,1);
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

void uiDisplayCurrentTempCallback() {
  uint16_t textColor;
  uint16_t textBackGround;
  textBackGround= ST77XX_BLACK;
  if(uiHeaterError) {
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
  if(isnan(env_temperature)) {
    tft.print("--.-");
  } else {
    tft.print(env_temperature,1);
  }
  tft.print(char(0xF7));
  tft.println("C");
}


void uiDisplayCurrentHumidityCallback() {
  uint16_t textColor;
  uint16_t textBackGround;
  textBackGround= ST77XX_BLACK;
  if(uiHeaterError) {
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
  if(isnan(env_humidity)) {
    tft.print("--.-");
  } else {
    tft.print(env_humidity,1);
  }
  tft.print("%");
  if(uiHeaterEnabled) {
    tft.println(char(0x07));
  } else {
    if(heater_loop_heartbeat) {
      tft.println(char(0x09));
    } else {
      tft.println(" ");
    }
  }  
}

void uiDisplayCurrentSetPointCallback() {
  // and now display the set temperature
  tft.setCursor(10, 120);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(3);
  tft.print("TEMP. DORITA ");
  tft.setCursor(0, 150);
  tft.setTextSize(5);
  tft.print(" ");
  tft.print(tempSetPoint,1);
  tft.print(char(0xF7));
  tft.println("C ");
}

void uiDisplayDebugDataCallback() {
  // display some debug data
  tft.setCursor(0, 205);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.print("Aux: ");
  if(isnan(heater_temperature)) {
    tft.print("--.-");
  } else {
    tft.print(heater_temperature,1);
  }
  tft.print(char(0xF7));
  tft.print("C, ");
  if(isnan(heater_humidity)) {
    tft.print("--.-");
  } else {
    tft.print(heater_humidity,1);
  }
  tft.print("%  ");
  tft.println("");
  tft.println("debug line 2");
}
