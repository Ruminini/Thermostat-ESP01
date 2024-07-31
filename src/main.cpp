#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#define RELAYPIN 2
#define DHTPIN 0
#define DHTTYPE DHT11

const char* ssid = "SSID";
const char* password = "PASSWORD";

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

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(RELAYPIN, OUTPUT);

  WiFi.hostname("ESP-Thermostat");
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

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
    lastRead = millis();
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (!isnan(temperature)) tem = temperature;
    if (!isnan(humidity)) hum = humidity;
    Serial.print("Temperature: " + String(tem) + " Humidity: " + String(hum));
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>ESP-01 Thermostat</title></head><body>";
  html += "<h1>ESP-01 Thermostat</h1>";
  html += "<p>Temperature: <span id='temperature'></span>&#8451;</p>";
  html += "<p>Humidity: <span id='humidity'></span>%</p>";
  html += "<p>Relay State: <span id='relayState'></span></p>";
  html += "<button type='submit' onclick='toggleRelay()'>Turn <span id='relayButton'></span></button>";
  html += "<script>";
  html += "let relay = 'OFF';";
  html += "function toggleRelay() {";
  html += "  fetch('/set?relay=' + (relay == 'ON' ? '1' : '0')).then(fetchData);";
  html += "}";
  html += "function fetchData() {";
  html += "  fetch('/data').then(response => response.json()).then(data => {";
  html += "    relay = data.relay == 1 ? 'OFF' : 'ON';";
  html += "    document.getElementById('temperature').innerText = data.temperature;";
  html += "    document.getElementById('humidity').innerText = data.humidity;";
  html += "    document.getElementById('relayState').innerText = relay;";
  html += "    document.getElementById('relayButton').innerText = data.relay == 1 ? 'ON' : 'OFF';";
  html += "  });";
  html += "}";
  html += "setInterval(fetchData, 5000);";
  html += "</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleData() {

  int relay = digitalRead(RELAYPIN);

  Serial.print("Temperature: ");
  Serial.println(tem);
  Serial.print("Humidity: ");
  Serial.println(hum);
  Serial.print("Relay: ");
  Serial.println(relay);

  String json = "{";
  json += "\"temperature\":";
  json += tem;
  json += ",";
  json += "\"humidity\":";
  json += hum;
  json += ",";
  json += "\"relay\":";
  json += relay;
  json += "}";

  server.send(200, "application/json", json);
}

void handleSet() {
  Serial.println(server.arg("relay"));
  int relay = server.arg("relay").toInt();
  digitalWrite(RELAYPIN, relay);
  server.send(200, "text/plain", String(relay));
}