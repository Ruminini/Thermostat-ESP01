# ESP-01 Thermostat 🌡
A simple and cheap thermostat controlled by an esp8266 with a web interface.

## Requirements

**Hardware:**
* **ESP-01** or any other esp8266, esp32's might need some code changes
* **DHT11 sensor** or DHT22/AM2301/AM2302/AM2321  
* **Relay module** If using a low level trigger, a rectifying diode is needed to stop 3v3 from being detected as low. If you just use a relay, a transistor is needed to trigger it.
* **USB to TTL programmer** for flashing (im using an Arduino Uno)
* 3v3 and 5v Power supply
* Cables
* Button


**Software:**
* VSCode with platformio extension
* library adafruit/DHT sensor 
* library bblanchon/ArduinoJson
* library esphome/ESPAsyncWebServer-esphome

## Assembly

Follow the schematic to assemble the thermostat.

![schematic](assets/schematic.png)

## Configure

### In config.h
* Set your Wi-Fi SSID and password.

### In main.cpp
* Confiure local IP, gateway and subnet.
* If using other sensor than DHT11, set DHTTYPE to the corresponding value.

## Flashing

To flash the code, inside platformio:
* Go to **Project Tasks** -> **ESP-01** -> **General** and **Upload**
the code
* Go to **Project Tasks** -> **ESP-01** -> **Platform** and upload the **Filesystem Image**.

## Usage

**Usage:**
* Access the thermostat's web interface to adjust settings.
* Temperature and humidity readings are displayed.
* Heating can be turned on/off manually.

**Contributing:**
Contributions are welcome! Please open an issue or pull request.

## TODO
* Add schematic.
* Persist schedule on power loss.
* Add more config options to web interface.
* Create AP when no Wi-Fi SSID and password are provided connection fails or button press on boot.
* Improve file configuration.
* Improve README.md
* Go back to EspWebServer instead of ESPAsyncWebServer for lower memory usage.