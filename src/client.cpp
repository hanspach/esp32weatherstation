#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHTesp.h>
#include <cJSON.h>
#include <U8g2lib.h>

#define DHT_PIN 18
#define CLOUDY 64
#define PARTLY_CLOUDY 65
#define NIGHT 66
#define RAINY 67
#define STARRY 68
#define SUNNY 69

const char*  ssid    = "Vodafone-CF6C";
const char* password = "7HGZ2eGXrTFpbGLE";
const char* addr = "http://api.openweathermap.org/data/2.5/weather?q=Dresden,DE&lang=de&APPID=41be67d28f623524876fb85fdc8f5cb3";
const String sPrefix("4c0002154d6fc88bbe756698da486866a36ec78e");

struct weather_data {
  int humidity;
  int pressure; 
  int temperature; 
  int clouds;
  String weather;
  String description;
};

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
static char buffer[36];
static TaskHandle_t xHandle = NULL;
struct tm dt;
struct weather_data wd;
DHTesp dht;
TempAndHumidity tah;
unsigned long t1;
bool validDate = false;
bool validForecast = false;
bool validMeasure = false;

void request(void* param) {
  struct tm dtWeb;
  HTTPClient http;

  while(!validDate || !validForecast) {
    configTime(3600,3600,"de.pool.ntp.org");
    if(getLocalTime(&dtWeb)) {
      validDate = true;
      Serial.println("validDate");
      dt = dtWeb;
      ++dt.tm_mon;
      dt.tm_year %= 100;
    }
    if(http.begin(addr)) {
      validForecast = http.GET() == HTTP_CODE_OK;
      if(validForecast) {
          String js = http.getString();
          cJSON* root = cJSON_Parse(js.c_str());
          cJSON* main = cJSON_GetObjectItem(root, "main"); 
          wd.humidity = cJSON_GetObjectItem(main, "humidity")->valueint;
          float t = cJSON_GetObjectItem(main, "temp")->valuedouble;
          wd.temperature = (int)(t - 273.15);
          wd.pressure = cJSON_GetObjectItem(main, "pressure")->valueint;
          cJSON* ar = cJSON_GetObjectItem(root, "weather");
          cJSON* weather = cJSON_GetArrayItem(ar, 0);
          wd.weather = cJSON_GetObjectItem(weather, "main")->valuestring;
          wd.description = cJSON_GetObjectItem(weather, "description")->valuestring;
          cJSON* clouds =  cJSON_GetObjectItem(root, "clouds");
          wd.clouds = cJSON_GetObjectItem(clouds, "all")->valueint;
          cJSON_Delete(root);
      }
    }
    delay(1000);
  }
  vTaskSuspend(NULL);
}

void printTime() {
  static const char* DAYS[] = {"","Mo","Di","Mi","Do","Fr","Sa","So"};
  static uint8_t x;
  static uint8_t y;
  
  u8g2.clearBuffer();
  sprintf(buffer,"%d:%02d ",dt.tm_hour,dt.tm_min);
  u8g2.setFontPosTop();
  x = 0;
  if(!validDate) {
	y = 13;
    u8g2.setFont(u8g2_font_logisoso32_tn);
  }
  else {
    u8g2.setFont(u8g2_font_logisoso24_tn);  // numbers only
	  y = 0;
  }
  u8g2.drawStr(x,y, buffer);
  x += u8g2.getStrWidth(buffer); 
  if(x > 100)
    x = 100;
  y += u8g2.getMaxCharHeight();
  u8g2.setFont(u8g2_font_7x14_tf);
  y -= u8g2.getMaxCharHeight(); 
  
  sprintf(buffer,"%02d",dt.tm_sec);
  u8g2.drawStr(x,y, buffer);
  if(validDate) {
    if(dt.tm_wday > 0 && dt.tm_wday <= 7) 
      sprintf(buffer,"%s, %d.%d.%02d",DAYS[dt.tm_wday],dt.tm_mday,dt.tm_mon,dt.tm_year);
    else
      sprintf(buffer,"%d.%d.%02d",dt.tm_mday,dt.tm_mon,dt.tm_year);
   
    u8g2.setFont(u8g2_font_9x18_tf);
    u8g2.setFontPosBottom();
    x = (u8g2.getDisplayWidth() - u8g2.getStrWidth(buffer)) / 2;
    u8g2.drawStr(x, u8g2.getDisplayHeight(), buffer); 
  } 
  u8g2.sendBuffer();

}

void printWeather() {
  static uint8_t pos;
  static uint8_t x;
  static bool firstTime = true;
  if(firstTime) {
         Serial.print("Vorhersage ");
        if(validDate) {
          sprintf(buffer, "aktualisiert um %d:%d",dt.tm_hour,dt.tm_min);
          Serial.println(buffer);
        }
        sprintf(buffer, "%02d°C %03d W:%d",wd.temperature,wd.humidity,wd.clouds);
        Serial.println(buffer);
        Serial.print("Weather:");
        Serial.println(wd.weather);
        Serial.print("Description:");
        Serial.println(wd.description);
        firstTime = false;
  }
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  sprintf(buffer, "%d",wd.temperature);
  u8g2.setFont(u8g2_font_t0_16_mf);
  u8g2.drawStr(0,0, buffer);
  x = u8g2.getStrWidth(buffer) + 2;
  uint8_t h = u8g2.getMaxCharHeight() + 5;
  u8g2.setFont(u8g2_font_t0_12_mf);
  u8g2.drawGlyph(x,2, 176);
  x += 5;
  u8g2.drawStr(x,0, "C");
  x = u8g2.getDisplayWidth() - u8g2.getStrWidth("%");
  u8g2.drawStr(x,4, "%");
  sprintf(buffer,"%d", wd.humidity);
  u8g2.setFont(u8g2_font_t0_16_mf);
  x -= u8g2.getStrWidth(buffer);
  u8g2.drawStr(x,4,buffer);
  if(dht.getStatus() == 0) {
    u8g2.setFontMode(1);   //tansparent
    u8g2.setDrawColor(1);
    u8g2.drawBox(0,h,u8g2.getDisplayWidth(),u8g2.getDisplayHeight()-2*h);
    u8g2.setFontPosCenter();
    u8g2.setFont(u8g2_font_open_iconic_gui_1x_t);
    uint8_t y = u8g2.getDisplayHeight()/2;
    u8g2.setDrawColor(0);
    u8g2.drawGlyph(0,y, 64);
    dtostrf(tah.temperature, 3,1,buffer);
    u8g2.setFont(u8g2_font_t0_16_mf);
    x = 16;
    u8g2.drawStr(x,y, buffer);      // Innentemparatur
    x += u8g2.getStrWidth(buffer) + 2;
    u8g2.setFont(u8g2_font_t0_12_mf);
    u8g2.drawGlyph(x,y, 176);
    x += 5;
    u8g2.drawStr(x,y, "C");
  }
  else {
    Serial.println(dht.getStatusString());
  }     
  u8g2.setDrawColor(1);
    u8g2.setFontPosBottom();
    if(wd.weather.equals("Clear")) {
      pos = SUNNY;
    }
    else if(wd.weather.equals("Clouds")) {
      if(wd.clouds < 50)
        pos = PARTLY_CLOUDY;
    else
        pos = CLOUDY;
    }
    u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
    u8g2.drawGlyph(0,u8g2.getDisplayHeight(), pos);
    
  x = 40;
  
  sprintf(buffer,"%s", "mBar");
  u8g2.setFont(u8g2_font_t0_12_mf);
  x = u8g2.getDisplayWidth() - u8g2.getStrWidth(buffer);
  u8g2.drawStr(x,u8g2.getDisplayHeight(), buffer);
  u8g2.setFont(u8g2_font_t0_16_mf);
  sprintf(buffer,"%d", wd.pressure);
  x -= u8g2.getStrWidth(buffer);
  u8g2.drawStr(x,u8g2.getDisplayHeight(), buffer);
  u8g2.sendBuffer(); 
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  WiFi.begin(ssid, password);
  u8g2.begin();
  dht.setup(DHT_PIN, DHTesp::AM2302);
  xTaskCreate(request,"Request",8192,NULL,1,&xHandle);            
  t1 = millis();
}

void loop() {
   static unsigned long t2;
  
  t2 = millis();
  if((t2 - t1)  > 995) {
     if(++dt.tm_sec > 59) {
       dt.tm_sec = 0;
       if(++dt.tm_min > 59) {
         dt.tm_min = 0;
          if(xHandle)
            vTaskResume(xHandle);
          if(++dt.tm_hour > 23) {
            dt.tm_hour = 0;
          } 
        }
     }
     if(validForecast && dt.tm_sec > 30 && dt.tm_sec < 59)
       printWeather();
     else
       printTime();
     t1 = t2;
  }
  delay(10);
}
