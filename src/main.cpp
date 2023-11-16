#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SPIFFS.h"
#include "LittleFS.h"
#include <Arduino_JSON.h>

const char* ssid = "The_internet";
const char* password = "Hm4p5m59";

const int oneWireBus = 4;     
OneWire oneWire(oneWireBus);
String latestItmes;

DallasTemperature sensors(&oneWire);

AsyncWebServer server(80);

AsyncWebSocket ws("/ws");
JSONVar readings;
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;


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

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      ws.textAll(latestItmes);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
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
        break;  
      }
    }

    file.close();
  } else {
    Serial.println("Error opening file for reading");
  }

  latestItmes = data;
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

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html", false);
  });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", read_temp("TEMPC").c_str());
  });

  server.on("/historical_data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", latestItmes);
  });

   server.serveStatic("/", SPIFFS, "/");
  server.begin();
}
 
void loop(){
  float temperature = read_temp("TEMPC").toFloat();
  logTemperature(temperature);
  syncHistoricalData();

  if ((millis() - lastTime) > timerDelay) {
    String sensorReadings = latestItmes;
    Serial.print(sensorReadings);
    notifyClients(sensorReadings);
    lastTime = millis();
  }


  ws.cleanupClients();
  delay(5000);
}