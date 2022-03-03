// general libraries
#include <SPI.h>

//display
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#define TFT_CS        -1
#define TFT_RST        8 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         9
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

//DHT22
#include "DHT.h"
#define DHT_TYPE  DHT22
#define DHT_POWER   7
#define DHT_1_PIN   6
#define DHT_2_PIN   5
DHT dht_heater(DHT_1_PIN, DHT_TYPE);
DHT dht_env(DHT_2_PIN, DHT_TYPE);
float heater_humidity;
float heater_temperature;
float env_humidity;
float env_temperature;



float p = 3.1415926;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // configure DHT pins and power both sensors
  pinMode(DHT_POWER,OUTPUT);
  digitalWrite(DHT_POWER,HIGH);
  dht_heater.begin();
  dht_env.begin();

  tft.setSPISpeed(1000);
  tft.init(240, 240,SPI_MODE2);
  tft.fillScreen(ST77XX_BLACK);
}


void tftPrintTest() {
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextSize(4);
  tft.print(1234.567);
  delay(1500);
  tft.setCursor(0, 0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(0);
  tft.println("Hello World!");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(p, 6);
  tft.println(" Want pi?");
  tft.println(" ");
  tft.print(8675309, HEX); // print 8,675,309 out in HEX!
  tft.println(" Print HEX!");
  tft.println(" ");
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Sketch has been");
  tft.println("running for: ");
  tft.setTextColor(ST77XX_MAGENTA);
  tft.print(millis() / 1000);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(" seconds.");
}

void loop() {
  // read temperature sensors
  heater_humidity = dht_heater.readHumidity();
  heater_temperature = dht_heater.readTemperature();
  env_humidity = dht_env.readHumidity();
  if(isnan(env_humidity)) {
    env_humidity = 99.0;
  }
  env_temperature = dht_env.readTemperature();
  if(isnan(env_temperature)) {
    env_temperature = 99.0;
  }

  // display the data
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setTextSize(4);
  tft.print("T:");
  tft.print(heater_temperature,1);
  tft.print("C ");
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.print(env_temperature,2);
  tft.print("C");
  tft.setTextSize(4);
  tft.println("");
  
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setTextSize(4);
  tft.print("H:");
  tft.print(heater_humidity,1);
  tft.print("% ");
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.print(env_humidity,1);
  tft.println("%");
  
  delay(2000);
  
}

void testdrawtext(char *text, uint16_t color) {
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}
