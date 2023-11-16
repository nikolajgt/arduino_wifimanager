/** \file */ 

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

const char* ssid = "The_internet"; /**< WiFi SSID */
const char* password = "Hm4p5m59"; /**< WiFi password */

const int oneWireBus = 4; /**< GPIO pin for the OneWire bus */
OneWire oneWire(oneWireBus); /**< OneWire object for temperature sensor */

String latestItmes; /**< String to store the latest temperature readings */

DallasTemperature sensors(&oneWire); /**< Dallas Temperature sensor object */

AsyncWebServer server(80); /**< AsyncWebServer instance */

AsyncWebSocket ws("/ws"); /**< AsyncWebSocket instance for WebSocket communication */

JSONVar readings; /**< JSON object to store temperature readings */

unsigned long lastTime = 0; /**< Timestamp for last data notification */
unsigned long timerDelay = 30000; /**< Delay for data notification (in milliseconds) */


/**
 * @brief Read temperature from the sensor.
 * @param var The variable to read, either "TEMPC" or "TEMPF".
 * @return A string representing the temperature in Celsius or Fahrenheit.
 */
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

/**
 * @brief Notify all WebSocket clients with sensor readings.
 * @param sensorReadings The sensor readings to send to clients.
 */
void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

/**
 * @brief Handles incoming WebSocket messages and responds with historical data.
 *
 * This function processes WebSocket messages received from clients. It validates
 * and handles incoming messages by sending the 'latestItmes' (historical temperature data)
 * as a WebSocket response to clients.
 *
 * @param arg   Pointer to WebSocket frame information.
 * @param data  Pointer to the received message data.
 * @param len   Length of the received message data.
 */
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      ws.textAll(latestItmes);
  }
}

/**
 * @brief Handles WebSocket server events, such as client connections and disconnections.
 *
 * This function serves as an event handler for WebSocket server events. It logs
 * client connections and disconnections, processes data events by delegating
 * message handling to 'handleWebSocketMessage()', and handles other WebSocket-related events.
 *
 * @param server  Pointer to the WebSocket server instance.
 * @param client  Pointer to the WebSocket client instance.
 * @param type    Type of WebSocket event (e.g., WS_EVT_CONNECT, WS_EVT_DISCONNECT).
 * @param arg     Argument data related to the event.
 * @param data    Pointer to the event data.
 * @param len     Length of the event data.
 */
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

/**
 * @brief Initialize WebSocket communication.
 */
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

/**
 * @brief Log temperature readings to a file.
 * @param temperature The temperature reading to log.
 */
void logTemperature(float temperature) {
    File file = SD.open("/temperature_log.txt", FILE_APPEND);
    if (file) {
        file.println(temperature);
        file.close();
    } else {
        Serial.println("Error opening file for writing");
    }
}


/**
 * @brief Synchronizes historical temperature data for serving to clients.
 *
 * This function reads historical temperature data from the SD card and synchronizes
 * it with the 'latestItmes' variable. It ensures that the number of historical
 * readings does not exceed the maximum limit defined by 'maxItems'.
 */
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

/**
 * @brief Initialize SPIFFS (SPI Flash File System).
 */
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}


/**
 * @brief Initializes the ESP32 and sets up required components.
 *
 * This function performs the following tasks:
 * 1. Initializes serial communication for debugging.
 * 2. Initializes the Dallas Temperature sensor and OneWire communication.
 * 3. Sets up the SPIFFS file system for serving web content and configuration files.
 * 4. Initializes a WebSocket server for real-time communication.
 * 5. Establishes a Wi-Fi connection with the specified credentials.
 * 6. Initializes SD card communication for data storage.
 * 7. Sets up HTTP server endpoints for handling requests.
 */
void setup(){
  Serial.begin(115200);
  sensors.begin();
  initSPIFFS();
  initWebSocket();
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
    notifyClients(sensorReadings);
    lastTime = millis();
  }


  ws.cleanupClients();
  delay(5000);
}