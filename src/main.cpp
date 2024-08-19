#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <config.h>

float tem;
float hum;
ulong lastSensorRead;
uint sensorReadInterval;
JsonDocument schedule;
DHT *dht;
uint hysteresis;
uint updateInterval;
int forceState;
int relay_pin;
bool configChanged = false;

AsyncWebServer server(80);

void readDht();
void regulateTemp();
void handleRoot(AsyncWebServerRequest *request);
void handleData(AsyncWebServerRequest *request);
void handleSet(AsyncWebServerRequest *request);
void handleSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleGetSchedule(AsyncWebServerRequest *request);
bool validateSchedule(JsonArray schedule);
bool validateTime(JsonArray time);
int compareTime(int hour1, int minute1, int hour2, int minute2);
int compareTime(JsonArray time1, JsonArray time2);
int compareTimeRange(tm *now, JsonDocument range);
void updateTargetTemp();
void saveConfig();
void handleSetupConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleGetSetupConfig(AsyncWebServerRequest *request);
bool collectJson(JsonDocument *json, AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

void setupWifi(JsonDocument config) {
  WiFi.hostname("ESP-Thermostat");
  String ssid = config["wifi_ssid"].as<String>();
  String pwd = config["wifi_pwd"].as<String>();
  IPAddress local_IP, gateway, subnet, primaryDNS, secondaryDNS;
  if (!config["dhcp"].as<bool>()) {
    local_IP.fromString(config["local_ip"].as<String>());
    gateway.fromString(config["gateway"].as<String>());
    subnet.fromString(config["subnet"].as<String>());
    if (config.containsKey("primary_dns"))
      primaryDNS.fromString(config["primary_dns"].as<String>());
    if (config.containsKey("secondary_dns"))
      secondaryDNS.fromString(config["secondary_dns"].as<String>());
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
      Serial.println("STA Failed to configure");
  }

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pwd);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void setupTime(JsonDocument config) {
  int timezone = config["ntp_timezone"].as<int>();
  int daylight = config["ntp_daylight"].as<int>();
  String ntpServer1 = config["ntp_server1"].as<String>();
  String ntpServer2 = config["ntp_server2"].as<String>();
  configTime(timezone * 3600, daylight, ntpServer1, ntpServer2);
}

void setupRelay(JsonDocument config) {
  relay_pin = config["relay_pin"];
  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, 1);
}

void setupSensor(JsonDocument config) {
  dht = new DHT(config["sensor_pin"], config["sensor_type"]);
  dht->begin();
  sensorReadInterval = config["sensor_read_interval"];
}

void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
  }
  JsonDocument config;
  File configFile = LittleFS.open("/config.json", "r");
  deserializeJson(config, configFile);
  serializeJsonPretty(config, Serial);
  configFile.close();
  schedule = config["schedule"];
  hysteresis = config["hysteresis"];
  updateInterval = config["updateInterval"];
  forceState = config["forceState"];

  JsonDocument setupConfig;
  File setupConfigFile = LittleFS.open("/setupConfig.json", "r");
  deserializeJson(setupConfig, setupConfigFile);
  serializeJsonPretty(setupConfig, Serial);
  configFile.close();

  setupWifi(setupConfig);
  setupTime(setupConfig);
  setupRelay(setupConfig);
  setupSensor(setupConfig);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSchedule);
  server.on("/schedule", HTTP_GET, handleGetSchedule);
  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleSetupConfig);
  server.on("/config", HTTP_GET, handleGetSetupConfig);

  server.onNotFound([](AsyncWebServerRequest *request) {
    String url = request->url();
    Serial.println("GET Not Found:\n" + url);
    request->send(LittleFS, url);
  });

  server.begin();
  Serial.println("Server started");
  Serial.println(WiFi.localIP());
}

void loop() {
  readDht();
  regulateTemp();
  saveConfig();
}

void readDht() {
  if (millis() - lastSensorRead < sensorReadInterval)
    return;
  lastSensorRead = millis();
  float temperature = dht->readTemperature();
  float humidity = dht->readHumidity();
  if (!isnan(temperature))
    tem = temperature;
  if (!isnan(humidity))
    hum = humidity;
  Serial.println("Temperature: " + String(temperature) + " Humidity: " + String(humidity));
}

unsigned long lastUpdate;
unsigned int targetTemp = 20;
void regulateTemp() {
  int newState = 1 - digitalRead(relay_pin);
  if (forceState > 1) {
    targetTemp = forceState;
  } else
    updateTargetTemp();
  if (forceState == 0 || forceState == 1) {
    newState = forceState;
  } else if (targetTemp == 0 || tem > targetTemp + (float)hysteresis / 2) {
    newState = 0;
  } else if (tem < targetTemp - (float)hysteresis / 2) {
    newState = 1;
  }
  if (newState == 1 - digitalRead(relay_pin))
    return;
  if (newState == 1 && millis() - lastUpdate < updateInterval)
    return;
  lastUpdate = millis();
  digitalWrite(relay_pin, 1 - newState);
}

JsonDocument lastSchedule;
void updateTargetTemp() {
  time_t now = time(nullptr);
  struct tm *timeinfo;
  timeinfo = localtime(&now);
  if (!lastSchedule.isNull() && compareTimeRange(timeinfo, lastSchedule) == 0) {
    targetTemp = (int)lastSchedule["temperature"];
    return;
  }
  JsonArray temps = schedule[String(timeinfo->tm_wday + 1)];
  for (JsonObject item : temps) {
    int inRange = compareTimeRange(timeinfo, item);
    Serial.println(inRange + " " + (int)item["temperature"]);
    if (inRange == 0) {
      targetTemp = item["temperature"];
      lastSchedule = item;
      return;
    } else if (inRange == -1) {
      lastSchedule = item;
      break;
    }
  }
  lastSchedule["end_time"] = lastSchedule["start_time"];
  lastSchedule["start_time"][0] = timeinfo->tm_hour;
  lastSchedule["start_time"][1] = timeinfo->tm_min;
  lastSchedule["temperature"] = 0;
  targetTemp = 0;
}

void handleRoot(AsyncWebServerRequest *request) {
  Serial.println("GET Root");
  request->send(LittleFS, "/index.html", "text/html");
}

void handleData(AsyncWebServerRequest *request) {
  Serial.println("GET Data");
  JsonDocument json;
  json["temperature"] = tem;
  json["humidity"] = hum;
  json["relay"] = 1 - digitalRead(relay_pin);;
  json["forceState"] = forceState;
  json["targetTemp"] = targetTemp;
  json["hysteresis"] = hysteresis;
  json["updateInterval"] = updateInterval;
  String response;
  serializeJson(json, response);
  request->send(200, "application/json", response);
}

void handleSet(AsyncWebServerRequest *request) {
  Serial.println("POST Set:");
  for (uint i = 0; i < request->args(); i++) {
    Serial.println(request->argName(i) + ": " + request->arg(i));
  }
  String error;
  String ok;
  if (request->hasArg("force")) {
    int nForce = request->arg("force").toInt();
    if ((nForce >= -1 && nForce <= 1) || (nForce >= 15 && nForce <= 30)) {
      forceState = nForce;
      ok += "Force state changed to " + String(nForce) + "\n";
    } else
      error += "Force must be -1(Auto), 0(Off), 1(On) or a temp between 15 and 30\n";
  }
  if (request->hasArg("hysteresis")) {
    int nHysteresis = request->arg("hysteresis").toInt();
    if (nHysteresis >= 0 && nHysteresis <= 10) {
      hysteresis = nHysteresis;
      ok += "Hysteresis changed to " + String(hysteresis) + "\n";
    } else
      error += "Hysteresis must be between 0 and 10\n";
  }
  if (request->hasArg("updateInterval")) {
    int nUpdateInterval = request->arg("updateInterval").toInt();
    if (nUpdateInterval >= 1000 && nUpdateInterval <= 600000) {
      updateInterval = nUpdateInterval;
      ok += "Update period changed to " + String(updateInterval) + "\n";
    } else
      error += "Update period must be between 1000 and 600000\n";
  }

  if (error == "") {
    configChanged = true;
    request->send(200, "text/json", "{\"success\":true, \"message\":\"" + ok + "\"}");
  } else
    request->send(400, "text/plain", error);
}

void handleSchedule(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  JsonDocument json;
  if (!collectJson(&json, request, data, len, index, total))
    return;

  // Example: {"schedule": [{"start_time": [0, 0], "end_time": [8, 0], "temperature": 20},{"start_time": [8, 0], "end_time": [16, 0], "temperature": 25},{"start_time": [16, 0], "end_time": [23,59], "temperature": 20}]}
  String path = request->url();
  if (path == "/schedule/single") {
    if (!validateSchedule(json["schedule"])) {
      request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid schedule\"}");
      return;
    }
    schedule.clear();
    lastSchedule.clear();
    for (int day = 1; day <= 7; day++) {
      schedule[(String)day] = json["schedule"];
    }
    request->send(200, "text/json", "{\"success\":true, \"schedule\": \"single\"}");
  }
  // Example: {"weekdays": [{"start_time": [0, 0], "end_time": [8, 0], "temperature": 20},{"start_time": [8, 0], "end_time": [16, 0], "temperature": 25},{"start_time": [16, 0], "end_time": [23, 59], "temperature": 20}], "weekends": [{"start_time": [0, 0], "end_time": [10, 0], "temperature": 20},{"start_time": [10, 0], "end_time": [16, 0], "temperature": 18},{"start_time": [16, 0], "end_time": [23, 59], "temperature": 22}]}
  if (path == "/schedule/weekly") {
    if (!json.containsKey("weekdays") ||
        !json.containsKey("weekends") ||
        !validateSchedule(json["weekdays"]) ||
        !validateSchedule(json["weekends"])) {
      request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid schedule\"}");
      return;
    }
    schedule.clear();
    lastSchedule.clear();
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
      if (!json.containsKey((String)day) || !validateSchedule(json[(String)day])) {
        request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid schedule\"}");
        return;
      }
    }
    schedule.clear();
    lastSchedule.clear();
    for (int day = 1; day <= 7; day++) {
      schedule[(String)day] = json[(String)day];
    }
    request->send(200, "text/json", "{\"success\":true}");
  }
  configChanged = true;
}

bool validateSchedule(JsonArray schedule) {
  JsonObject prevItem;
  for (JsonObject item : schedule) {
    if (!item["temperature"].is<float>())
      return false;
    if (item["temperature"] < 15 || item["temperature"] > 30)
      return false;
    if (!validateTime(item["start_time"]) || !validateTime(item["end_time"]))
      return false;
    if (compareTime(item["start_time"], item["end_time"]) == 1)
      return false;
    if (prevItem) {
      if (compareTime(prevItem["end_time"], item["start_time"]) == 1)
        return false;
    }
    prevItem = item;
  }
  return true;
}

bool validateTime(JsonArray time) {
  if (time.size() != 2)
    return false;
  if (!time[0].is<int>() || !time[1].is<int>())
    return false;
  if (time[0] < 0 || time[0] > 24)
    return false;
  if (time[1] < 0 || time[1] > 59)
    return false;
  if (time[0] == 24 && time[1] != 0)
    return false;
  return true;
}

int compareTime(int hour1, int minute1, int hour2, int minute2) {
  if (hour1 < hour2)
    return -1;
  if (hour1 > hour2)
    return 1;
  if (minute1 < minute2)
    return -1;
  if (minute1 > minute2)
    return 1;
  return 0;
}

int compareTime(JsonArray time1, JsonArray time2) {
  return compareTime(time1[0], time1[1], time2[0], time2[1]);
}

int compareTimeRange(tm *now, JsonDocument range) {
  JsonArray start = range["start_time"];
  JsonArray end = range["end_time"];
  if (compareTime(now->tm_hour, now->tm_min, end[0], end[1]) == 1)
    return 1;
  if (compareTime(now->tm_hour, now->tm_min, start[0], start[1]) == -1)
    return -1;
  return 0;
}

void handleGetSchedule(AsyncWebServerRequest *request) {
  Serial.println("GET Schedule");
  String response;
  serializeJson(schedule, response);
  request->send(200, "text/json", response);
}

ulong lastSaveTime;
void saveConfig() {
  if (!configChanged || millis() - lastSaveTime < 60000)
    return;
  configChanged = false;
  lastSaveTime = millis();
  JsonDocument config;
  config["schedule"] = schedule;
  config["hysteresis"] = hysteresis;
  config["updateInterval"] = updateInterval;
  config["forceState"] = forceState;
  File configFile = LittleFS.open("/config.json", "w");
  serializeJson(config, configFile);
  configFile.close();
  Serial.println("Saved config");
}

void handleGetSetupConfig(AsyncWebServerRequest *request) {
  Serial.println("GET SetupConfig");
  request->send(LittleFS, "/setupConfig.json", "application/json");
}

void handleSetupConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  Serial.println("POST SetupConfig");
  int args = request->args();
  for(int i=0;i<args;i++){
    Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
  }
  JsonDocument json;
  if (!collectJson(&json, request, data, len, index, total))
    return;
  serializeJson(json, Serial);
  JsonDocument parsedJson;
  if (json.containsKey("wifi_ssid")) {
    parsedJson["wifi_ssid"] = json["wifi_ssid"];
    parsedJson["wifi_pwd"] = json["wifi_pwd"];
  }
  if (json.containsKey("dhcp")) {
    parsedJson["dhcp"] = json["dhcp"] == "on";
    if (json["dhcp"] == "off") {
      parsedJson["local_ip"] = json["local_ip"];
      parsedJson["gateway"] = json["gateway"];
      parsedJson["subnet"] = json["subnet"];
      if (json.containsKey("primary_dns"))
        parsedJson["primary_dns"] = json["primary_dns"];
      if (json.containsKey("secondary_dns"))
        parsedJson["secondary_dns"] = json["secondary_dns"];
    }
  }
  if (json.containsKey("ntp_server1")) {
    parsedJson["ntp_server1"] = json["ntp_server1"];
    if (json.containsKey("ntp_server2"))
      parsedJson["ntp_server2"] = json["ntp_server2"];
    if (json.containsKey("ntp_timezone")) {
      parsedJson["ntp_timezone"] = json["ntp_timezone"].as<int>();
      if (json.containsKey("ntp_daylight"))
        parsedJson["ntp_daylight"] = json["ntp_daylight"].as<int>();
    }
  }
  if (json.containsKey("relay_pin"))
    parsedJson["relay_pin"] = json["relay_pin"].as<int>();
  if (json.containsKey("sensor_pin"))
    parsedJson["sensor_pin"] = json["sensor_pin"].as<int>();
  if (json.containsKey("sensor_type"))
    parsedJson["sensor_type"] = json["sensor_type"];
  if (json.containsKey("sensor_read_interval"))
    parsedJson["sensor_read_interval"] = json["sensor_read_interval"].as<int>();
  if (json.containsKey("config_save_interval"))
    parsedJson["config_save_interval"] = json["config_save_interval"].as<int>();

  if (json.containsKey("hysteresis"))
    parsedJson["hysteresis"] = json["hysteresis"].as<int>();
  if (json.containsKey("update_interval"))
    parsedJson["update_interval"] = json["update_interval"].as<int>();

  serializeJson(parsedJson, Serial);
  File setupConfig = LittleFS.open("/setupConfig.json", "w");
  serializeJson(parsedJson, setupConfig);
  setupConfig.close();
  request->send(200, "text/json", "{\"success\":true}");
  delay(1000);
  ESP.restart();
}

String partialJson;
bool collectJson(JsonDocument *json, AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  Serial.println("Collecting JSON");
  for (size_t i = 0; i < len; i++) {
    partialJson += (char)data[i];
  }
  Serial.println(partialJson);
  serializeJson(*json, Serial);
  if (index + len != total)
    return false;
  DeserializationError error = deserializeJson(*json, partialJson);
  Serial.println("Deserialized JSON");
  Serial.println(partialJson);
  serializeJson(*json, Serial);
  partialJson = "";
  if (error) {
    request->send(400, "text/json", "{\"success\":false, \"error\":\"Invalid JSON\"}");
    return false;
  }
  return true;
}
