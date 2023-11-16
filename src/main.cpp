#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SPIFFS.h"
#include "LittleFS.h"


const char* ssid = "The_internet";
const char* password = "Hm4p5m59";

const int oneWireBus = 4;     
OneWire oneWire(oneWireBus);
String latestItmes;

DallasTemperature sensors(&oneWire);

AsyncWebServer server(80);

String read_temp(const String& var) {
  sensors.requestTemperatures(); 
  if(var == "TEMPC")
  {
    return String(sensors.getTempCByIndex(0));
  }
  else{
     return String(sensors.getTempFByIndex(0));
  }
}

void logTemperature(float temperature) {
    File file = SD.open("/temperature_log.txt", FILE_APPEND);
    if (file) {
        file.println(temperature);
        file.close();
    } else {
        Serial.println("Error opening file for writing");
    }
}

String processor(const String& var){
  return read_temp(var);
}

void syncHistoricalData() {
  String data = "";

  int maxItems = 50;
  File file = SD.open("/temperature_log.txt", FILE_READ);
  
  if (file) {
    int startPos = 0;
    int endPos = 0;
    int lineCount = 0;

    file.seek(0, SeekEnd);
    
    while (lineCount < maxItems) {
      endPos = file.position();
      while (endPos > 0 && file.read() != '\n') {
        file.seek(endPos - 1);
        endPos = file.position();
      }

      String line = file.readStringUntil('\n');
      data = line + "\n" + data;
      lineCount++;

      if (endPos > 0) {
        file.seek(endPos - 1);
      } else {
        break;  // Reached the beginning of the file
      }
    }

    file.close();
  } else {
    Serial.println("Error opening file for reading");
  }

  latestItmes = data;
}


String getHistoricalData1() {
    return "22 \n";
}



void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}


void setup(){
  Serial.begin(115200);
  sensors.begin();
  initSPIFFS();
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  } 

  File file = SPIFFS.open("/index.html", "r");
  if(file.available())
  {
    Serial.println(file.name());
  }else {
    Serial.println("not found");
  }
  

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html", false);
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", read_temp("TEMPC").c_str());
  });

  server.on("/historical_data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", latestItmes);
  });


  server.begin();
}
 
void loop(){
  float temperature = read_temp("TEMPC").toFloat();
  logTemperature(temperature);
  syncHistoricalData();
  delay(5000);
  // String temp = read_temp("TEMPC");
  // Serial.println(temp);
}