#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <config.h>

#define RELAYPIN 2
#define DHTPIN 0
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
float tem;
float hum;
int lastRead;

ESP8266WebServer server(80);

IPAddress local_IP(192, 168, 1, 11);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

void readDht();
void handleRoot();
void handleData();
void handleSet();

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(RELAYPIN, OUTPUT);

  WiFi.hostname("ESP-Thermostat");
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/set", handleSet);

  server.begin();
  Serial.println("Server started");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  readDht();
}

void readDht() {
  if (millis() - lastRead < 5000) return;
  Serial.println("lastRead: " + String(lastRead) + " millis()" + String(millis()));
  lastRead = millis();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (!isnan(temperature)) tem = temperature;
  if (!isnan(humidity)) hum = humidity;
  Serial.println("Temperature: " + String(temperature) + " Humidity: " + String(humidity));
}

void handleRoot() {
  String html = R""""(<!DOCTYPE html><html><head><title>ESP-01 Thermostat</title></head><body>
  <h1>ESP-01 Thermostat</h1>
  <p>Temperature: <span id='temperature'></span>&#8451;</p>
  <p>Humidity: <span id='humidity'></span>%</p>
  <p>Relay State: <span id='relayState'></span></p>
  <button type='submit' onclick='toggleRelay()'>Turn <span id='relayButton'></span></button>
  <script>
  let relay = 'OFF';
  function toggleRelay() {
    fetch('/set?relay=' + (relay == 'ON' ? '1' : '0')).then(fetchData);
  }
  function fetchData() {
    fetch('/data').then(response => response.json()).then(data => {
      relay = data.relay == 1 ? 'OFF' : 'ON';
      document.getElementById('temperature').innerText = data.temperature;
      document.getElementById('humidity').innerText = data.humidity;
      document.getElementById('relayState').innerText = relay;
      document.getElementById('relayButton').innerText = data.relay == 1 ? 'ON' : 'OFF';
    });
  }
  setInterval(fetchData, 5000);
  </script>
  </body></html>)"""";
  server.send(200, "text/html", html);
}

void handleData() {
  int relay = digitalRead(RELAYPIN);
  String json = "{\"temperature\":" + String(tem) + " , \"humidity\":" + String(hum) + " , \"relay\":" + String(relay) + "}";
  Serial.println(json);
  server.send(200, "application/json", json);
}

void handleSet() {
  if (server.hasArg("relay")) {
    int relay = server.arg("relay").toInt();
    if (relay == 0 || relay == 1) {
      digitalWrite(RELAYPIN, relay);
    } else server.send(400, "text/plain", "Relay must be 0 or 1");
  }
  server.send(200, "text/plain", "OK");
}