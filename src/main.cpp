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
unsigned long int lastRead;

ESP8266WebServer server(80);

IPAddress local_IP(192, 168, 1, 11);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

void readDht();
void regulateTemp();
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
unsigned int hysteresis = 2;
unsigned int updatePeriod = 15000;
int forceState = -1;
void regulateTemp() {
  if (millis() - lastUpdate < updatePeriod) return;
  lastUpdate = millis();
  if (forceState != -1) {
    digitalWrite(RELAYPIN, 1 - forceState);
  } else  if (tem < targetTemp - hysteresis/2) {
    digitalWrite(RELAYPIN, 0);
  } else if (tem > targetTemp + hysteresis/2) {
    digitalWrite(RELAYPIN, 1);
  }
}

void handleRoot() {
  String html = R""""(
<!DOCTYPE html>
<html>
<head>
	<title>ESP-01 Thermostat</title>
</head>
<body>
	<h1>ESP-01 Thermostat</h1>
	<p>Temperature: <span id='temperature'></span>&#8451;</p>
	<p>Humidity: <span id='humidity'></span>%</p>
	<p>Relay State: <span id='relayState'></span></p>
	<label for="force">Force</label>
	<input onclick='toggleForce()' type="checkbox" id="force">
	<button id="relay" onclick='toggleRelay()'>On</button>
	<label for="targetTemp">Target Temperature: <span id="targetTempValue">20</span></label>
	<input type="range" id="targetTemp" min="15" max="30" step="1" value="20" oninput="updateValue('targetTemp')">
	<label for="hysteresis">Hysteresis: <span id="hysteresisValue">2</span></label>
	<input type="range" id="hysteresis" min="0" max="10" step="1" value="2" oninput="updateValue('hysteresis')">
	<label for="updatePeriod">Update Period: <span id="updatePeriodValue">300</span>sec</label>
	<input type="range" id="updatePeriod" min="15" max="600" step="15" value="300"  oninput="updateValue('updatePeriod')">
	<script>
		function toggleForce() {
			const force = document.getElementById('force').checked;
			document.getElementById('relay').disabled = !force;
			document.getElementById('targetTemp').disabled = force;
			document.getElementById('hysteresis').disabled = force;
			document.getElementById('updatePeriod').disabled = force;
			toggleRelay(force ? relay == 'ON' ? 1 : 0 : -1);
		}
		let relay = 'OFF';
		let updateForm = true;
		function toggleRelay(state = relay == 'ON' ? 0 : 1) {
			fetch('/set?relay=' + state).then(fetchData);
		}
		let debounceTimer;
		function updateValue(id) {
				const element = document.getElementById(id);
				const valueElement = document.getElementById(id + 'Value');
				valueElement.textContent = element.value;
				clearTimeout(debounceTimer);
				debounceTimer = setTimeout(() => {
					fetch('/set?' + id + '=' + element.value);
				}, 500);
		}
		function fetchData() {
			fetch('/data').then(response => response.json()).then(data => {
				relay = data.relay == 0 ? 'OFF' : 'ON';
				document.getElementById('temperature').innerText = data.temperature;
				document.getElementById('humidity').innerText = data.humidity;
				document.getElementById('relayState').innerText = relay;
				document.getElementById('relay').innerText = relay;
				const targetTemp = data.targetTemp;
				const hysteresis = data.hysteresis;
				const updatePeriod = data.updatePeriod/1000;
				document.getElementById('targetTemp').value = targetTemp;
				document.getElementById('hysteresis').value = hysteresis;
				document.getElementById('updatePeriod').value = updatePeriod;
				updateValue('targetTemp');
				updateValue('hysteresis');
				updateValue('updatePeriod');
			});
		}
		setInterval(fetchData, 5000);
		fetchData();
		toggleForce();
	</script>
</body>
</html>
)"""";
  server.send(200, "text/html", html);
}

void handleData() {
  int relay = forceState!= -1 ? forceState : 1 - digitalRead(RELAYPIN);
  String json = "{\"temperature\":" + String(tem) +
  " , \"humidity\":" + String(hum) +
  " , \"relay\":" + String(relay) +
  " , \"targetTemp\":" + String(targetTemp) +
  " , \"hysteresis\":" + String(hysteresis) +
  " , \"updatePeriod\":" + String(updatePeriod) +
  "}";
  Serial.println(json);
  server.send(200, "application/json", json);
}

void handleSet() {
  String error;
  if (server.hasArg("relay")) {
    int relay = server.arg("relay").toInt();
    if (relay >= -1 && relay <= 1) {
      forceState = relay;
      Serial.println("Relay state changed to " + String(relay));
    } else error += "Relay must be -1(Auto), 0(Off) or 1(On)\n";
  }
  if (server.hasArg("targetTemp")) {
    int nTargetTemp = server.arg("targetTemp").toInt();
    if (targetTemp >= 15 && targetTemp <= 30) {
      targetTemp = nTargetTemp;
      Serial.println("Target temperature changed to " + String(targetTemp));
    } else error += "Target temperature must be between 15 and 30\n";
  }
  if (server.hasArg("hysteresis")) {
    int nHysteresis = server.arg("hysteresis").toInt();
    if (hysteresis >= 0 && hysteresis <= 10) {
      hysteresis = nHysteresis;
      Serial.println("Hysteresis changed to " + String(hysteresis));
    } else error += "Hysteresis must be between 0 and 10\n";
  }
  if (server.hasArg("updatePeriod")) {
    int nUpdatePeriod = server.arg("updatePeriod").toInt();
    if (updatePeriod >= 1000 && updatePeriod <= 600000) {
      updatePeriod = nUpdatePeriod;
      Serial.println("Update period changed to " + String(updatePeriod));
    } else error += "Update period must be between 1000 and 600000\n";
  }
  if (error != "") {
    server.send(400, "text/plain", error);
  } else server.send(200, "text/plain", "OK");
}