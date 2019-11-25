<p align="center">
<img src=".github/loading-screen.gif">
</p>

# Home Assistant - TEMPERATURE SENSOR

**Temperature_Sensor_with_Oled_0.69"**

**This is PCB for a small temperature senzor with a 0.96" oled display.**

**The software is not made by me I just adapted to my needs, you can make the same.**

**It can be integrated in Home Assistant.**

# Hardware  <a name="id3"></a>

The PCB was ordered from JLCPC 
- **https://jlcpcb.com**

You can find the link also the EasyEDA File

- **https://easyeda.com/tiraalexandru/DHT-Oled-tmp-sensor**

**PCB Top**                      |  **PCB_Bottom**   
:---------------------------:|:-------------------------------------:
<img src=".github/PCB.PNG">|  <img align="right" src=".github/PCB_Bottom.PNG">


# BOM <a name="id3"></a>
 
 **Name** | **Designator** | **Quantity**
:---: | :--------: | --------:
096"  |OLED-SSD1306|1
WEMOS D1 MINI |WEMOS D1 MINI V2.3.0 1.74MM|1
DHT22|U1|1

# Library Needed <a name="id3"></a>
**Name** | **Link** | **Version** 
:------: | :------: | -----------:
NTPClient | [GIT](https://github.com/arduino-libraries/NTPClient) |**`latest`**
ESP8266WebServer | [GIT](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer)| **`latest`**
WiFiManager | [GIT](https://github.com/tzapu/WiFiManager) | **`latest`**
Arduino Json | [ARDUINO IDE](https://arduinojson.org) |**`6.x.`**
PubSubClient | [GIT](https://github.com/knolleary/pubsubclient)| **`2.7.x`**
DHT | [GIT](https://github.com/adafruit/DHT-sensor-library) | **`latest`**
U8g2lib - 0.96" oled| [ARDUINO LIB](https://www.arduinolibraries.info/libraries/u8g2 ) | **`U8g2-2.26.14`**
OneWire | [ARDUINO LIB](https://www.arduinolibraries.info/libraries/one-wire) | **`OneWire-2.3.5`**
DallasTemperature| [GIT](https://github.com/milesburton/Arduino-Temperature-Control-Library) | **`latest`**
Adafruit_HTU21DF| [GIT](https://github.com/adafruit/Adafruit_HTU21DF_Library) | **`latest`**
DallasTemperature| [GIT](https://github.com/milesburton/Arduino-Temperature-Control-Library) | **`latest`**
Adafruit_HTU21DF| [GIT](https://github.com/adafruit/Adafruit_HTU21DF_Library) | **`latest`**
Adafruit_APDS9960|[GIT](https://github.com/adafruit/Adafruit_APDS9960) | **`latest`**
Adafruit_Sensor| Instal from library **"Adafruit Unified Sensor"** by Adafruit | **`1.0.2`**
Adafruit_BMP085_U| [GIT](https://github.com/adafruit/Adafruit_BMP085_Unified) | **`latest`**
