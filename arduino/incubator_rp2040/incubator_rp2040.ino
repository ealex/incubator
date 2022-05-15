// general libraries
#include <SPI.h>
#include "pico/stdlib.h"

#include <FreeRTOS.h>
#include "task.h"
static const UBaseType_t uxCore0AffinityMask = 0x01;
static const UBaseType_t uxCore1AffinityMask = 0x02;


/*********** CORE 0 TASKS ****************************/
// led blink task
void TaskBlink( void *pvParameters );
TaskHandle_t xTaskBlinkHandle;
volatile bool ledStatus;

//display and user interface
#include <U8g2lib.h>
U8G2_ST7565_EA_DOGM128_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 17, /* dc=*/ 21, /* reset=*/ 20);
#define TASK_DISPLAY_MAX_DELAY  (TickType_t)(5000)
void vTaskDisplay( void *pvParameters );
TaskHandle_t xTaskDisplayHandle;

// button handling
#include "Button2.h"
#define BUTTON_UP_PIN   13
#define BUTTON_X_PIN    14
#define BUTTON_DOWN_PIN 15
Button2 buttonUp, buttonX, buttonDown;
void click(Button2& btn);




/*********** CORE 1 TASKS ****************************/
// temperature sensor
#include "DHT.h"
#define DHT_TYPE  DHT22
#define DHT_PIN   (27)
DHT tempSensor(DHT_PIN, DHT_TYPE);
volatile float heater_humidity;
volatile float heater_temperature;
#define TASK_DHT_DELAY  (TickType_t)(1000)
void vTaskDHT(void *pvParameters);
TaskHandle_t xTaskDHTHandle;

// motor control task
#define MOTOR_PIN   (22)
#define TASK_MOTOR_DELAY  (TickType_t)(100000) //run the task once per 100s
volatile uint8_t motorOnTime =0; // how long is the motor on, from 0 to 100 (s) 
void vTaskMotor(void *pvParameters);
TaskHandle_t xTaskMotorHandle;

// pid control task
#include <PID_v1.h>
#define PID_PIN (26)
#define TASK_PID_DELAY  (TickType_t)(1000)
void vTaskPid(void *pvParameters);
TaskHandle_t xTaskPidHandle;
double consKp=1, consKi=0.05, consKd=0.25;
double Setpoint, Input, Output;
PID myPID(&Input, &Output, &Setpoint, consKp, consKi, consKd, DIRECT);

// adc read task
volatile float vBat;
#define TASK_ADC_DELAY  (TickType_t)(1000)
#define ADC_PIN   (28)
void vTaskADC(void *pvParameters);
TaskHandle_t xTaskADCHandle;



//************************* CODE STARTS HERE *********************
void setup() {    
  // setup code for core 0
  Serial.begin();

  // SPI port used by the display
  SPI.setSCK(18);
  SPI.setTX(19);
  SPI.begin(false);

  // get all tasks up and running
  // status led blink task, core 0, low prio
  xTaskCreate( TaskBlink, (const portCHAR *)"TaskBlink",  1024,  NULL ,  6,  &xTaskBlinkHandle );
  vTaskCoreAffinitySet(xTaskBlinkHandle, uxCore0AffinityMask);

  // display task - core 0 low prio
  xTaskCreate( vTaskDisplay, (const portCHAR *)"vTaskDisplay",  1024,  NULL ,  7,  &xTaskDisplayHandle );
  vTaskCoreAffinitySet(xTaskDisplayHandle, uxCore0AffinityMask);


  // DHT update CORE 1
  xTaskCreate( vTaskDHT, (const portCHAR *)"vTaskDHT",  1024,  NULL ,  2,  &xTaskDHTHandle );
  vTaskCoreAffinitySet(xTaskDHTHandle, uxCore1AffinityMask);

  // egg turner control CORE 1
  xTaskCreate( vTaskMotor, (const portCHAR *)"vTaskMotor",  1024,  NULL ,  7,  &xTaskMotorHandle );
  vTaskCoreAffinitySet(xTaskMotorHandle, uxCore1AffinityMask);

  // PID controller
  xTaskCreate( vTaskPid, (const portCHAR *)"vTaskPid",  1024,  NULL ,  2,  &xTaskPidHandle );
  vTaskCoreAffinitySet(xTaskPidHandle, uxCore1AffinityMask);

  // VBAT adc
  //xTaskCreate( vTaskADC, (const portCHAR *)"vTaskADC",  1024,  NULL ,  6,  &xTaskADCHandle );
  //vTaskCoreAffinitySet(xTaskADCHandle, uxCore1AffinityMask);


  // start ui buttons
  buttonUp.begin(BUTTON_UP_PIN);
  buttonUp.setClickHandler(click);

  buttonX.begin(BUTTON_X_PIN);
  buttonX.setClickHandler(click);

  buttonDown.begin(BUTTON_DOWN_PIN);
  buttonDown.setClickHandler(click);

}


/***  CORE 0 related code ***/
// task to control the LED, low priority
void TaskBlink( void *pvParameters ) {
  TickType_t xLastExecutionTime = xTaskGetTickCount();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);
  
  while(true) {
    if(ledStatus) {
      digitalWrite(LED_BUILTIN,HIGH);
      ledStatus = false;
    } else {
      digitalWrite(LED_BUILTIN,LOW);
      ledStatus = true;
    }

    // wait some time
    // xTaskNotify(xTaskDisplayHandle, 0x00, eSetValueWithOverwrite);
    vTaskDelayUntil( &xLastExecutionTime, 1000);
  }

  // should not reach this point
  vTaskDelete( NULL );
}


/**  
 *   This task will handle the user interface and display refresh
 *   the task stays disable untill is woken up by one of the following triggers:
 *    - DHT22 data read
 *    - button press (up / down / action)
 *    - power supply voltage adc read
 *    - system status monitor (power supply status, 
 */
void vTaskDisplay( void *pvParameters ) {
  TickType_t xLastExecutionTime = xTaskGetTickCount();
  char displayTextBuffer[64];
  u8g2.begin();
  u8g2.setDisplayRotation(U8G2_R2);
  u8g2.setContrast(100);
  u8g2.setFontDirection(0);
  while(true) {
    // wait for a notification
    ulTaskNotifyTake( pdTRUE, portMAX_DELAY);

    //redraw screen
    u8g2.clearBuffer();
    u8g2.setFontMode(0);
    u8g2.setDrawColor(1);

    
    // draw the current temperature and humidity
    u8g2.setFont(u8g2_font_lubB14_tf);  
    sprintf(displayTextBuffer,"%02.1f%cC", heater_temperature, 0xb0);
    u8g2.drawStr(2,16,displayTextBuffer);  // write something to the internal memory
    u8g2.drawVLine(64,0,18);
    sprintf(displayTextBuffer,"%02.1f%%",heater_humidity);
    u8g2.drawStr(67,16,displayTextBuffer);  // write something to the internal memory
    u8g2.drawHLine(0,18,128);

    // draw egg turner settings and status
    u8g2.setFont(u8g2_font_6x12_tf);
    sprintf(displayTextBuffer, "Motor %03d/100", motorOnTime);
    u8g2.drawStr(2, 28, displayTextBuffer);
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(90, 28, 66); // motor ON
    
    u8g2.setFont(u8g2_font_6x12_tf);
    sprintf(displayTextBuffer,"Output:%03.0f", Output);
    u8g2.drawStr(2, 36, displayTextBuffer);

    u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
    u8g2.drawGlyph(0, 60, 73); // battery full
    u8g2.drawGlyph(128/2-8, 60, 64); // battery
    u8g2.drawGlyph(128-16, 60, 71); // warning

    // draw the screen
    u8g2.sendBuffer();
  }  
  vTaskDelete( NULL );
}


/**
 * idle loop
 *  runs the button loops over here
 */
void loop() {
  // only buttons handling here
  buttonUp.loop();
  buttonX.loop();
  buttonDown.loop();
}

/**
 * buttons action callback
 *  this will set some status flags
 *  it will notify the display task
 */
void click(Button2& btn) {
  // TODO
}




/***  CORE 1 related code ***/

/**
 * updates the humidty and temperature data from the DHT sensor
 *  - this task runs once every second
 *  - on read it will notify the following tasks:
 *      - xTaskDisplay via xTaskDisplayHandle
 *      - xTaskPID  via xTaskPIDHandle
 *  - if any of the 2 values are not readable it will set the general alarm flag
 */
void vTaskDHT(void *pvParameters) {
  // configure DHT pins
  pinMode(DHT_PIN, INPUT);
  digitalWrite(DHT_PIN, HIGH);
  tempSensor.begin();
  
  TickType_t xLastExecutionTime = xTaskGetTickCount();
  while(true) {
    heater_temperature = tempSensor.readTemperature();
    heater_humidity = tempSensor.readHumidity();
    
    // notify the display task
    xTaskNotify(xTaskDisplayHandle, 0x00, eSetValueWithOverwrite);
    
    // notify the PID task
    // TODO

    // sleep until next interval
    vTaskDelayUntil( &xLastExecutionTime, TASK_DHT_DELAY);
  }
  vTaskDelete( NULL );
}


// control the egg turner motor
void vTaskMotor(void *pvParameters) {
  TickType_t xLastExecutionTime = xTaskGetTickCount();
  uint32_t runTime;
  uint32_t sleepTile;
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN,LOW);
  
  while(true) {
    runTime = motorOnTime * 1000;
    sleepTile = TASK_MOTOR_DELAY - runTime;
    if(0==motorOnTime) {
      // motor is disabled
      digitalWrite(MOTOR_PIN,LOW);
      vTaskDelayUntil( &xLastExecutionTime, TASK_MOTOR_DELAY);
    } else {
      // turn the motor on
      digitalWrite(MOTOR_PIN,HIGH);
      vTaskDelayUntil( &xLastExecutionTime, runTime);
      digitalWrite(MOTOR_PIN,LOW);
      vTaskDelayUntil( &xLastExecutionTime, sleepTile);
    }
  }
  vTaskDelete( NULL );
}


/**
 * this task updates the PID loop
 *  it's output goes straight to the PWM that drives the power transistors
 *  the PWM frequency is set to 100Hz - to reduce EMI / long wire issues
 */
void vTaskPid(void *pvParameters) {
  TickType_t xLastExecutionTime = xTaskGetTickCount();
  Setpoint = 37.5;
  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(TASK_PID_DELAY);
  pinMode(PID_PIN, OUTPUT);

  // starts the PWM output
  analogWriteFreq(100); // 100Hz
  analogWriteRange(255);  // from 0 to 255
  
  while(true) {
    Input = heater_temperature;
    myPID.Compute();
    analogWrite( PID_PIN, Output);
    vTaskDelayUntil( &xLastExecutionTime, TASK_PID_DELAY);
  }
  vTaskDelete( NULL );
}


/**
 * 
 */
void vTaskADC(void *pvParameters) {
  TickType_t xLastExecutionTime = xTaskGetTickCount();
  while(true) {
    vBat = analogRead(ADC_PIN);  
    vTaskDelayUntil( &xLastExecutionTime, TASK_ADC_DELAY);
  }
}
