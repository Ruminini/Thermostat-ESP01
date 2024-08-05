#include <Arduino.h>
#include <ArduinoJson.h>
#include <config.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#define RELAYPIN 2
#define DHTPIN 0
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
float tem;
float hum;
unsigned long int lastRead;
JsonDocument schedule;

AsyncWebServer server(80);

IPAddress local_IP(192, 168, 1, 11);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

void readDht();
void regulateTemp();
void handleRoot(AsyncWebServerRequest *request);
void handleData(AsyncWebServerRequest *request);
void handleSet(AsyncWebServerRequest *request);
void handleSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
bool validateSchedule(JsonArray schedule);
bool validateTime(JsonArray time);
int compareTime(int hour1, int minute1, int hour2, int minute2);
int compareTime(JsonArray time1, JsonArray time2);
int compareTimeRange(tm * now, JsonObject range);
void updateTargetTemp();

void setup() {
  Serial.begin(115200);
  dht.begin();
  if (!LittleFS.begin()) {
  Serial.println("An Error has occurred while mounting LittleFS");
  }
  configTime(-3 * 3600, 0, "ntp.inti.gob.ar", "pool.ntp.org");
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, 1);

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

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/schedule", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleSchedule);

  server.onNotFound([](AsyncWebServerRequest *request){
    String url = request->url();
    request->send(LittleFS, url, "text/"+url.substring(url.lastIndexOf('.') + 1));
  });

  server.begin();
  Serial.println("Server started");
  Serial.println(WiFi.localIP());
}

void loop() {
  readDht();
  regulateTemp();
}

unsigned int readSleep = 2500;
void readDht() {
  if (millis() - lastRead < readSleep) return;
  lastRead = millis();
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (!isnan(temperature)) tem = temperature;
  if (!isnan(humidity)) hum = humidity;
  Serial.println("Temperature: " + String(temperature) + " Humidity: " + String(humidity));
}

unsigned long lastUpdate;
unsigned int targetTemp = 20;
unsigned int hysteresis = 1;
unsigned int updatePeriod = 15000;
int forceState = -1;
void regulateTemp() {
  if (millis() - lastUpdate < updatePeriod) return;
  lastUpdate = millis();
  if (forceState == 0 || forceState == 1) {
    digitalWrite(RELAYPIN, 1 - forceState);
    return;
  } else if (forceState > 1){
    targetTemp = forceState;
  } else updateTargetTemp();
  if (targetTemp == 0 || tem > targetTemp + (float)hysteresis/2) {
    digitalWrite(RELAYPIN, 1);
  } else if (tem < targetTemp - (float)hysteresis/2) {
    digitalWrite(RELAYPIN, 0);
  }
  serializeJsonPretty(schedule["7"], Serial);
}

void updateTargetTemp() {
  time_t now = time(nullptr);
  struct tm * timeinfo;
  timeinfo = localtime(&now);
  JsonArray temps = schedule[String(timeinfo->tm_wday+1)];
  for (JsonObject item: temps) {
    int inRange = compareTimeRange(timeinfo, item);
    Serial.println(inRange);
    Serial.println((int)item["temperature"]);
    if (inRange == 0) {
      targetTemp = item["temperature"];
      return;
    } else if (inRange == -1) {
      break;
    }
  }
  targetTemp = 0;
}

void handleRoot(AsyncWebServerRequest *request) {
  request->send(LittleFS, "/index.html", "text/html");
}

void handleData(AsyncWebServerRequest *request) {
  int relay = 1 - digitalRead(RELAYPIN);
  String json = "{\"temperature\":" + String(tem) +
  " , \"humidity\":" + String(hum) +
  " , \"relay\":" + String(relay) +
  " , \"targetTemp\":" + String(targetTemp) +
  " , \"hysteresis\":" + String(hysteresis) +
  " , \"updatePeriod\":" + String(updatePeriod) +
  "}";
  request->send(200, "application/json", json);
}

void handleSet(AsyncWebServerRequest *request) {
  String error;
  String ok;
  if (request->hasArg("force")) {
    int nForce = request->arg("force").toInt();
    if ((nForce >= -1 && nForce <= 1) || (nForce >= 15 && nForce <= 30)) {
      forceState = nForce;
      ok += "Force state changed to " + String(nForce) + "\n";
    } else error += "Force must be -1(Auto), 0(Off), 1(On) or a temp between 15 and 30\n";
  }
  if (request->hasArg("hysteresis")) {
    int nHysteresis = request->arg("hysteresis").toInt();
    if (nHysteresis >= 0 && nHysteresis <= 10) {
      hysteresis = nHysteresis;
      ok += "Hysteresis changed to " + String(hysteresis) + "\n";
    } else error += "Hysteresis must be between 0 and 10\n";
  }
  if (request->hasArg("updatePeriod")) {
    int nUpdatePeriod = request->arg("updatePeriod").toInt();
    if (nUpdatePeriod >= 1000 && nUpdatePeriod <= 600000) {
      updatePeriod = nUpdatePeriod;
      ok += "Update period changed to " + String(updatePeriod) + "\n";
    } else error += "Update period must be between 1000 and 600000\n";
  }

  if (error == "") {
    request->send(200, "text/json", "{\"success\":true, \"message\":\"" + ok + "\"}");
  } else request->send(400, "text/plain", error);
}

String jsonData;
void handleSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  for (size_t i = 0; i < len; i++) {
    jsonData += (char)data[i];
  }
  if (index + len != total) return;
  JsonDocument json;
  DeserializationError error = deserializeJson(json, jsonData);
  jsonData = "";
  if (error) {
    request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid JSON\"}");
    return;
  }
  // Example: {"schedule": [{"start_time": [0, 0], "end_time": [8, 0], "temperature": 20},{"start_time": [8, 0], "end_time": [16, 0], "temperature": 25},{"start_time": [16, 0], "end_time": [23,59], "temperature": 20}]}
  String path = request->url();
  if (path == "/schedule/single") {
    if (!validateSchedule(json["schedule"])) {
      request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid schedule\"}");
      return;
    }
    schedule.clear();
    for (int day = 1; day <= 7; day++) {
      schedule[(String)day] = json["schedule"];
    }
    request->send(200, "text/json", "{\"success\":true, \"schedule\": \"single\"}");
  }
  // Example: {"weekdays": [{"start_time": [0, 0], "end_time": [8, 0], "temperature": 20},{"start_time": [8, 0], "end_time": [16, 0], "temperature": 25},{"start_time": [16, 0], "end_time": [23, 59], "temperature": 20}], "weekends": [{"start_time": [0, 0], "end_time": [10, 0], "temperature": 20},{"start_time": [10, 0], "end_time": [16, 0], "temperature": 18},{"start_time": [16, 0], "end_time": [23, 59], "temperature": 22}]}
  if (path == "/schedule/weekly") {
    if ( !json.containsKey("weekdays") ||
         !json.containsKey("weekends") ||
         !validateSchedule(json["weekdays"]) ||
         !validateSchedule(json["weekends"])) {
      request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid schedule\"}");
      return;
    }
    schedule.clear();
    schedule["1"] = json["weekends"];
    schedule["7"] = json["weekends"];
    for (int day = 2; day <= 6; day++) {
      schedule[(String)day] = json["weekdays"];
    }
    request->send(200, "text/json", "{\"success\":true}");
  }
  // Example: {"1": [{"start_time": [0, 0], "end_time": [8, 0], "temperature": 20}],"2":[{"start_time": [8, 0], "end_time": [16, 0], "temperature": 25}],"3":[{"start_time": [16, 0], "end_time": [23, 59], "temperature": 20}],"4": [{"start_time": [0, 0], "end_time": [8, 0], "temperature": 20}],"5":[{"start_time": [8, 0], "end_time": [16, 0], "temperature": 25}],"6":[{"start_time": [16, 0], "end_time": [23, 59], "temperature": 20}],"7": [{"start_time": [0, 0], "end_time": [23, 59], "temperature": 20}]}
  if (path == "/schedule/daily") {
    for (int day = 1; day <= 7; day++) {
      if ( !json.containsKey((String)day) || !validateSchedule(json[(String)day])) {
        request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid schedule\"}");
        return;
      }
    }
    schedule.clear();
    for (int day = 1; day <= 7; day++) {
      schedule[(String)day] = json[(String)day];
    }
    request->send(200, "text/json", "{\"success\":true}");
  }
}

bool validateSchedule(JsonArray schedule) {
  JsonObject prevItem;
  for (JsonObject item: schedule) {
    if (!item["temperature"].is<float>()) return false;
    if (item["temperature"] < 15 || item["temperature"] > 30) return false;
    if (!validateTime(item["start_time"]) || !validateTime(item["end_time"])) return false;
    if (compareTime(item["start_time"], item["end_time"]) != -1) return false;
    if (prevItem) {
      if (compareTime(prevItem["end_time"], item["start_time"]) == 1) return false;
    }
    prevItem = item;
  }
  return true;
}

bool validateTime(JsonArray time) {
  if (time.size() != 2) return false;
  if (!time[0].is<int>() || !time[1].is<int>()) return false;
  if (time[0] < 0 || time[0] > 24) return false;
  if (time[1] < 0 || time[1] > 59) return false;
  if (time[0] == 24 && time[1] != 0) return false;
  return true;
}

int compareTime(int hour1, int minute1, int hour2, int minute2) {
  if (hour1 < hour2) return -1;
  if (hour1 > hour2) return 1;
  if (minute1 < minute2) return -1;
  if (minute1 > minute2) return 1;
  return 0;
}

int compareTime(JsonArray time1, JsonArray time2) {
  return compareTime(time1[0], time1[1], time2[0], time2[1]);
}

int compareTimeRange(tm * now, JsonObject range) {
  JsonArray start = range["start_time"];
  JsonArray end = range["end_time"];
  if (compareTime(now->tm_hour, now->tm_min, end[0], end[1]) == 1) return 1;
  if (compareTime(now->tm_hour, now->tm_min, start[0], start[1]) == -1) return -1;
  return 0;
}