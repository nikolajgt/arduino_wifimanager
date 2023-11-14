#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <SPIFFS.h>


const char* ssid = "The_internet";
const char* password = "Hm4p5m59";

const int oneWireBus = 4;     
OneWire oneWire(oneWireBus);

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

String getHistoricalData() {
  String data = "";
  File file = SD.open("/temperature_log.txt", FILE_READ);
  if (file) {
    while (file.available()) {
      data += file.readStringUntil('\n') + "\n";
    }
    file.close();
  } else {
    Serial.println("Error opening file for reading");
  }
  return data;
}


void setup(){
  Serial.begin(115200);
  sensors.begin();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());

  // if (!SD.begin()) {
  //   Serial.println("Card Mount Failed");
  //   return;
  // }
  // uint8_t cardType = SD.cardType();

  // if (cardType == CARD_NONE) {
  //   Serial.println("No SD card attached");
  //   return;
  // } 

 server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    File file = SPIFFS.open("/data/index.html", "r");
    if (!file) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    String htmlContent = "";
    while (file.available()) {
      htmlContent += file.readStringUntil('\n');
    }
    file.close();
    request->send(200, "text/html", htmlContent);
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", read_temp("TEMPC").c_str());
  });

  server.on("/historical_data", HTTP_GET, [](AsyncWebServerRequest *request){
    String historicalData = getHistoricalData();
    request->send(200, "text/plain", historicalData);
  });


  server.begin();
}
 
void loop(){
  float temperature = read_temp("TEMPC").toFloat();
  logTemperature(temperature);
  delay(1000);
}