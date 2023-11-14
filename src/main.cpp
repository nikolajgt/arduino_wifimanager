/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Replace with your network credentials
const char* ssid = "The_internet";
const char* password = "Hm4p5m59";


const int oneWireBus = 4;     
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Create AsyncWebServer object on port 80
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

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>ESP32 DHT Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <canvas id="temperatureChart" width="400" height="400"></canvas>
</body>
<script>
  const ctx = document.getElementById('temperatureChart').getContext('2d');
  const temperatureChart = new Chart(ctx, {
      type: 'line',
      data: {
          labels: [], // Time Labels
          datasets: [{
              label: 'Temperature',
              data: [], // Temperature data
              backgroundColor: 'rgba(255, 99, 132, 0.2)',
              borderColor: 'rgba(255, 99, 132, 1)',
              borderWidth: 1
          }]
      },
      options: {
          scales: {
              y: {
                  beginAtZero: true
              }
          }
      }
  });

  function addData(chart, label, data) {
      chart.data.labels.push(label);
      chart.data.datasets.forEach((dataset) => {
          dataset.data.push(data);
      });
      chart.update();
  }

  function fetchData() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        const historicalData = this.responseText.split('\n').filter(Boolean); // Remove empty lines
        historicalData.forEach((entry, index) => {
          const temperature = parseFloat(entry);
          const now = new Date(new Date().getTime() - index * 10000); // Simulate timestamps
          const timeString = now.getHours() + ':' + now.getMinutes() + ':' + now.getSeconds();
          addData(temperatureChart, timeString, temperature);
        });
      }
    };
    xhttp.open("GET", "/historical_data", true);
    xhttp.send();
  }

  // Fetch historical data when the page loads
  fetchData();

  // Fetch data every 10 seconds
  setInterval(fetchData, 10000);
</script>
</html>)rawliteral";

// Replaces placeholder with DHT values




void setup(){
  Serial.begin(115200);
  sensors.begin();

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
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", read_temp("TEMPC").c_str());
  });

  server.on("/historical_data", HTTP_GET, [](AsyncWebServerRequest *request){
    String historicalData = getHistoricalData();
    request->send(200, "text/plain", historicalData);
  });

  // Start server
  server.begin();
}
 
void loop(){
  float temperature = read_temp("TEMPC").toFloat();
  logTemperature(temperature);
  delay(1000);
}