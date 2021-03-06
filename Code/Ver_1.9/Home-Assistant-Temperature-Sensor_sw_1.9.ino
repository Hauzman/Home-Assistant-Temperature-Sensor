/*

  Home-Assistant-Temperature-Sensor

  If HOME_ASSISTANT_DISCOVERY is defined, the Hauzman Thermometer will
  publish MQTT messages that makes Home Assistant auto-discover the
  device.  See https://www.home-assistant.io/docs/mqtt/discovery/.

  If PUBLISH_CHIP_ID is defined, the Hauzman Thermometer will publish
  the chip ID using MQTT.  This can be considered a privacy issue,
  and is disabled by default.

  Should Over-the-Temperature upgrades be supported?  They are only supported
  if this define is set, and the user configures an OTA server on the
  wifi configuration page (or defines OTA_SERVER below).  If you use
  OTA you are strongly adviced to use signed builds (see
  https://arduino-esp8266.readthedocs.io/en/2.5.0/ota_updates/readme.html#advanced-security-signed-updates)

  The GPIO2 pin is connected to two things on the Hauzman Thermometer:

  - Internally on the ESP-12E module, it is connected to a blue
   status LED, that lights up whenever the pin is driven low.

  - On the Hauzman Thermometer board, it is connected to the DHT22
   temperature and humidity sensor.  Whenever a measurements is
   made, both the ESP8266 on the ESP-12E module and the DHT22 sensor
   drives the pin low.  One measurement drives it low several times:

      - At least 1 ms by the ESP8266 to start the measurement.
      - 80 us by the DHT22 to acknowledge
      - 50 us by the DHT22 for each bit transmitted, a total of 40 times

   In total, this means it is driven low more than 3 ms for each
   measurement.  This results in a blue flash from the status LED
   that is clearly visible, especially in a dark room.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#undef PUBLISH_CHIP_ID
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>
#include <NTPClient.h>             // --> https://github.com/arduino-libraries/NTPClient
#include <ArduinoJson.h>           // --> https://github.com/bblanchon/ArduinoJson
#include <MD5Builder.h>
#include <DHT.h>                   // Instal from library "DHT-sensor-library" by Adafruit Ver 1.3.4
#include <Wire.h>
#include <OneWire.h>               // --> https://www.arduinolibraries.info/libraries/one-wire ver OneWire-2.3.5
#include <DallasTemperature.h>     // --> https://github.com/milesburton/Arduino-Temperature-Control-Library
#include "Adafruit_HTU21DF.h"      // --> https://github.com/adafruit/Adafruit_HTU21DF_Library
#include "Adafruit_APDS9960.h"     // --> https://github.com/adafruit/Adafruit_APDS9960
#include <Adafruit_Sensor.h>       // Instal from library "Adafruit Unified Sensor" by Adafruit (version 1.0.2)
#include <Adafruit_BMP085_U.h>     // --> https://github.com/adafruit/Adafruit_BMP085_Unified
#include "Settings.h"
#include "mqtt.h"
#define VERSION "1.9"
/*******************************************************************************************/
WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 60000); // hare you can ajust the time "7200"


Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

/*******************************************************************************************
  Configure supported I2C sensors
*******************************************************************************************/
const int sensorHTU21D =  0x40; // Change the adress if needed
const int sensorBH1750 = 0x23;
const int sensorBMP180 = 0x77;
const int i2cDisplayAddress = 0x3c;
/*******************************************************************************************/
// Configure pins
const int pinAlarm = 16;
const int pinButton = 0;
unsigned long sensorPreviousMillis = 0;
const long sensorInterval = 10000;
unsigned long mqttConnectionPreviousMillis = millis();
const long mqttConnectionInterval = 60000;

// Set temperature coefficient for calibration depending on an empirical research with
// comparison to DS18B20 and other temperature sensors. You may need to adjust it for the
// specfic DHT22 unit on your board
float temperatureCoef = 0.9;
/*******************************************************************************************/
// Similar, for the DS18B20 sensor.
float dsTemperatureCoef = 1.0;
float dhtTemperature = 0;
float dhtHumidity = 0;
float dsTemperature = 0;
float sensorTemperature = 0;
float sensorHumidity = 0;
uint16_t sensorAmbientLight = 0;
/*******************************************************************************************/

/*******************************************************************************************/

#ifdef OTA_UPGRADES
char ota_server[40];
#endif
char temp_scale[40] = "celsius";
/*******************************************************************************************/
// Set the temperature in Celsius or Fahrenheit
// true - Celsius, false - Fahrenheit
bool configTempCelsius = true;

// MD5 of chip ID.  If you only have a handful of thermometers and use
// your own MQTT broker (instead of iot.eclips.org) you may want to
// truncate the MD5 by changing the 32 to a smaller value.
char machineId[32 + 1] = "";
/*******************************************************************************************/
//flag for saving data
bool shouldSaveConfig = false;

/*******************************************************************************************/
#ifdef OTA_UPGRADES
char cmnd_update_topic[12 + sizeof(machineId)];
#endif

char line1_topic[11 + sizeof(machineId)];
char line2_topic[11 + sizeof(machineId)];
char line3_topic[11 + sizeof(machineId)];
char cmnd_temp_coefficient_topic[14 + sizeof(machineId)];
char cmnd_ds_temp_coefficient_topic[20 + sizeof(machineId)];
char cmnd_temp_format[16 + sizeof(machineId)];
/*******************************************************************************************/
// The display can fit 26 "i":s on a single line.  It will fit even
// less of other characters.
char mqtt_line1[26 + 1];
char mqtt_line2[26 + 1];
char mqtt_line3[26 + 1];
String sensor_line1;
String sensor_line2;
String sensor_line3;
bool need_redraw = false;
char stat_temp_coefficient_topic[14 + sizeof(machineId)];
char stat_ds_temp_coefficient_topic[20 + sizeof(machineId)];
/*******************************************************************************************/
//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


/*******************************************************************************************/
void load_calibration()
{
  if (!SPIFFS.exists("/calibration.json"))
    return;
  File configFile = SPIFFS.open("/calibration.json", "r");
  if (!configFile)
    return;
  const size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument jsonBuffer(1024);
  auto error = deserializeJson(jsonBuffer, buf.get());
  Serial.print("Loading /calibration.json: ");
  serializeJson(jsonBuffer, Serial);
  Serial.println("");
  if (error)
    return;
  const char *val = jsonBuffer["dht22_temp_mult"];
  temperatureCoef = atof(val);
  val = jsonBuffer["ds18b20_temp_mult"];
  dsTemperatureCoef = atof(val);
  configFile.close();
  Serial.print("DHT22: ");
  Serial.println(temperatureCoef);
  Serial.print("DS18B20: ");
  Serial.println(dsTemperatureCoef);
}

void save_calibration()
{
  DynamicJsonDocument jsonBuffer(1024);
  char buf_a[40];
  char buf_b[40];
  snprintf(buf_a, sizeof(buf_a), "%g", temperatureCoef);
  snprintf(buf_b, sizeof(buf_b), "%g", dsTemperatureCoef);
  jsonBuffer["dht22_temp_mult"] = buf_a;
  jsonBuffer["ds18b20_temp_mult"] = buf_b;

  File configFile = SPIFFS.open("/calibration.json", "w");
  if (!configFile)
  {
    Serial.println("failed to open calibration file for writing");
    return;
  }

  serializeJson(jsonBuffer, Serial);
  Serial.println("");
  serializeJson(jsonBuffer, configFile);
  configFile.close();
}

void saveConfig()
{
  Serial.println("Saving configurations to file.");
  DynamicJsonDocument json(1024);
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["workgroup"] = workgroup;
  json["username"] = username;
  json["password"] = password;
  json["temp_scale"] = temp_scale;
#ifdef HOME_ASSISTANT_DISCOVERY
  json["ha_name"] = ha_name;
#endif
#ifdef OTA_UPGRADES
  json["ota_server"] = ota_server;
#endif

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    Serial.println("ERROR: failed to open config file for writing");
    return;
  }

  serializeJson(json, Serial);
  serializeJson(json, configFile);
  configFile.close();
}

void checkDisplay()
{
  Serial.print("Mini I2C OLED Display at address ");
  Serial.print(i2cDisplayAddress, HEX);
  if (isSensorAvailable(i2cDisplayAddress))
  {
    Serial.println(": OK");
  }
  else
  {
    Serial.println(": N/A");
  }
}

void setup()
{
  // put your setup code here, to run once:
  strcpy(mqtt_line1, "");
  strcpy(mqtt_line2, "");
  strcpy(mqtt_line3, "");
  need_redraw = true;
  Serial.begin(115200);
  Serial.println();

  Wire.begin();
  checkDisplay();

  timeClient.begin();
  u8g2.begin();
  dht.begin();
  sensors.begin();

  delay(10);

  //LED
  pinMode(pinAlarm, OUTPUT);
  //Button
  pinMode(pinButton, INPUT);

  waitForFactoryReset();

  // Machine ID
  calculateMachineId();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        const size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        if (DeserializationError::Ok == deserializeJson(json, buf.get()))
        {
          serializeJson(json, Serial);
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(workgroup, json["workgroup"]);
          strcpy(username, json["username"]);
          strcpy(password, json["password"]);
          {
            const char *s = json["temp_scale"];
            if (!s)
              s = "celsius";
            strcpy(temp_scale, s);
          }
#ifdef HOME_ASSISTANT_DISCOVERY
          {
            const char *s = json["ha_name"];
            if (!s)
              s = machineId;
            snprintf(ha_name, sizeof(ha_name), "%s", s);
          }
#endif
          
#ifdef OTA_UPGRADES
          {
            const char *s = json["ota_server"];
            if (!s)
              s = ""; // The empty string never matches.
            snprintf(ota_server, sizeof(ota_server), "%s", s);
          }
#endif
        }
        else
        {
          Serial.println("failed to load json config");
        }
      }
    }
    load_calibration();
  }
  else
  {
    Serial.println("failed to mount FS");
  }

  /*******************************************************************************************/
  // Set MQTT topics
  sprintf(line1_topic, "cmnd/%s/line1", machineId);
  sprintf(line2_topic, "cmnd/%s/line2", machineId);
  sprintf(line3_topic, "cmnd/%s/line3", machineId);
  sprintf(cmnd_temp_coefficient_topic, "cmnd/%s/tempcoef", machineId);
  sprintf(stat_temp_coefficient_topic, "stat/%s/tempcoef", machineId);
  sprintf(cmnd_ds_temp_coefficient_topic, "cmnd/%s/DS18B20/tempcoef", machineId);
  sprintf(stat_ds_temp_coefficient_topic, "stat/%s/DS18B20/tempcoef", machineId);
  sprintf(cmnd_temp_format, "cmnd/%s/tempformat", machineId);
#ifdef OTA_UPGRADES
  sprintf(cmnd_update_topic, "cmnd/%s/update", machineId);
#endif
  /*******************************************************************************************/
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, sizeof(mqtt_server));
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, sizeof(mqtt_port));
  WiFiManagerParameter custom_workgroup("workgroup", "workgroup", workgroup, sizeof(workgroup));
  WiFiManagerParameter custom_mqtt_user("user", "MQTT username", username, sizeof(username));
  WiFiManagerParameter custom_mqtt_pass("pass", "MQTT password", password, sizeof(password));
#ifdef HOME_ASSISTANT_DISCOVERY
  WiFiManagerParameter custom_mqtt_ha_name("ha_name", "Sensor name for Home Assistant", ha_name, sizeof(ha_name));
#endif
#ifdef OTA_UPGRADES
  WiFiManagerParameter custom_ota_server("ota_server", "OTA server", ota_server, sizeof(ota_server));
#endif
  WiFiManagerParameter custom_temperature_scale("temp_scale", "Temperature scale", temp_scale, sizeof(temp_scale));

  char htmlMachineId[200];
  sprintf(htmlMachineId, "<p style=\"color: red;\">Machine ID:</p><p><b>%s</b></p><p>Copy and save the machine ID because you will need it to control the device.</p>", machineId);
  WiFiManagerParameter custom_text_machine_id(htmlMachineId);
  /*******************************************************************************************
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
  *******************************************************************************************/
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_workgroup);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_temperature_scale);
#ifdef HOME_ASSISTANT_DISCOVERY
  wifiManager.addParameter(&custom_mqtt_ha_name);
#endif
#ifdef OTA_UPGRADES
  wifiManager.addParameter(&custom_ota_server);
#endif
  wifiManager.addParameter(&custom_text_machine_id);

  //reset settings - for testing
  //wifiManager.resetSettings();
  /*******************************************************************************************/
  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);

  digitalWrite(pinAlarm, HIGH);
  drawDisplay("Connecting...", WiFi.SSID().c_str());
  
  /*******************************************************************************************/
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Hauzman Thermometer", ""))
  {
    digitalWrite(pinAlarm, LOW);
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("Connected....");
  digitalWrite(pinAlarm, LOW);

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(workgroup, custom_workgroup.getValue());
  strcpy(username, custom_mqtt_user.getValue());
  strcpy(password, custom_mqtt_pass.getValue());
  strcpy(temp_scale, custom_temperature_scale.getValue());
#ifdef HOME_ASSISTANT_DISCOVERY
  strcpy(ha_name, custom_mqtt_ha_name.getValue());
#endif
#ifdef OTA_UPGRADES
  strcpy(ota_server, custom_ota_server.getValue());
#endif

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfig();
  }
  Serial.println("Software Version");
  Serial.println("Version: " + String(VERSION));
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  drawDisplay("Connected !", "Local IP:", WiFi.localIP().toString().c_str());
  delay(2000);
  /*******************************************************************************************/
  // Sensors
  htu.begin();
  bmp.begin();
  /*******************************************************************************************/
  // MQTT
  Serial.print("MQTT Server: ");
  Serial.println(mqtt_server);
  Serial.print("MQTT Port: ");
  Serial.println(mqtt_port);
  // Print MQTT Username
  Serial.print("MQTT Username: ");
  Serial.println(username);
  // Hide password from the log and show * instead
  char hiddenpass[20] = "";
  for (size_t charP = 0; charP < strlen(password); charP++)
  {
    hiddenpass[charP] = '*';
  }
  hiddenpass[strlen(password)] = '\0';
  Serial.print("MQTT Password: ");
  Serial.println(hiddenpass);
  Serial.print("Saved temperature scale: ");
  Serial.println(temp_scale);
  configTempCelsius = ( (0 == strlen(temp_scale)) || String(temp_scale).equalsIgnoreCase("celsius"));
  Serial.print("Temperature scale: ");
  if (true == configTempCelsius)
  {
    Serial.println("Celsius");
  }
  else
  {
    Serial.println("Fahrenheit");
  }
#ifdef HOME_ASSISTANT_DISCOVERY
  Serial.print("Home Assistant sensor name: ");
  Serial.println(ha_name);
#endif
#ifdef OTA_UPGRADES
  if (ota_server[0] != '\0')
  {
    Serial.print("OTA server: ");
    Serial.println(ota_server);
  }
  else
  {
#  ifndef OTA_SERVER
    Serial.println("No OTA server");
#  endif
  }

#  ifdef OTA_SERVER
  Serial.print("Hardcoded OTA server: ");
  Serial.println(OTA_SERVER);
#  endif

#endif

  const int mqttPort = atoi(mqtt_port);
  mqttClient.setServer(mqtt_server, mqttPort);
  mqttClient.setCallback(mqttCallback);

  mqttReconnect();

  Serial.println("");
  Serial.println("-----");
  Serial.print("Machine ID: ");
  Serial.println(machineId);
  Serial.println("-----");
  Serial.println("");

  setupADPS9960();
}

void setupADPS9960()
{
  if (apds.begin())
  {
//gesture mode will be entered once proximity mode senses something close
    apds.enableProximity(true);
    apds.enableGesture(true);
  }
}

void waitForFactoryReset()
{
  Serial.println("Press button within 2 seconds for factory reset...");
  for (int iter = 0; iter < 20; iter++)
  {
    digitalWrite(pinAlarm, HIGH);
    delay(50);
    if (false == digitalRead(pinButton))
    {
      factoryReset();
      return;
    }
    digitalWrite(pinAlarm, LOW);
    delay(50);
    if (false == digitalRead(pinButton))
    {
      factoryReset();
      return;
    }
  }
}

void factoryReset()
{
  if (false == digitalRead(pinButton))
  {
    Serial.println("Hold the button to reset to factory defaults...");
    bool cancel = false;
    for (int iter = 0; iter < 30; iter++)
    {
      digitalWrite(pinAlarm, HIGH);
      delay(100);
      if (true == digitalRead(pinButton))
      {
        cancel = true;
        break;
      }
      digitalWrite(pinAlarm, LOW);
      delay(100);
      if (true == digitalRead(pinButton))
      {
        cancel = true;
        break;
      }
    }
    if (false == digitalRead(pinButton) && !cancel)
    {
      digitalWrite(pinAlarm, HIGH);
      Serial.println("Disconnecting...");
      WiFi.disconnect();
      /*******************************************************************************************/
      // NOTE: the boot mode:(1,7) problem is known and only happens at the first restart after serial flashing.

      Serial.println("Restarting...");
      // Clean the file system with configurations
      SPIFFS.format();
      // Restart the board
      ESP.restart();
    }
    else
    {
      // Cancel reset to factory defaults
      Serial.println("Reset to factory defaults cancelled.");
      digitalWrite(pinAlarm, LOW);
    }
  }
}

#ifdef OTA_UPGRADES
void do_ota_upgrade(char *text)
{
  DynamicJsonDocument json(1024);
  auto error = deserializeJson(json, text);
  if (error)
  {
    Serial.println("No success decoding JSON.\n");
  }
  else if (!json.containsKey("server"))
  {
    Serial.println("JSON is missing server\n");
  }
  else if (!json.containsKey("file"))
  {
    Serial.println("JSON is missing file\n");
  }
  else
  {
    int port = 0;
    if (json.containsKey("port"))
    {
      port = json["port"];
      Serial.print("Port configured to ");
      Serial.println(port);
    }

    if (0 >= port || 65535 < port)
    {
      port = 80;
    }

    String server = json["server"];
    String file = json["file"];

    bool ok = false;
    if (ota_server[0] != '\0' && !strcmp(server.c_str(), ota_server))
      ok = true;

#  ifdef OTA_SERVER
    if (!strcmp(server.c_str(), OTA_SERVER))
      ok = true;
#  endif

    if (!ok)
    {
      Serial.println("Wrong OTA server. Refusing to upgrade.");
      return;
    }

    Serial.print("Attempting to upgrade from ");
    Serial.print(server);
    Serial.print(":");
    Serial.print(port);
    Serial.println(file);
    ESPhttpUpdate.setLedPin(pinAlarm, HIGH);
    WiFiClient update_client;
    t_httpUpdate_return ret = ESPhttpUpdate.update(update_client,
                              server, port, file);
    switch (ret)
    {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  }
}
#endif

void processMessageScale(const char* text)
{
  StaticJsonDocument<200> data;
  deserializeJson(data, text);
  // Set temperature to Celsius or Fahrenheit and redraw screen
  Serial.print("Changing the temperature scale to: ");
  if (data.containsKey("scale") && (0 == strcmp(data["scale"], "celsius")) )
  {
    Serial.println("Celsius");
    configTempCelsius = true;
    strcpy(temp_scale, "celsius");
  }
  else
  {
    Serial.println("Fahrenheit");
    configTempCelsius = false;
    strcpy(temp_scale, "fahrenheit");
  }
  // Force default sensor lines with the new format for temperature
  setDefaultSensorLines();
  need_redraw = true;
  // Save configurations to file
  saveConfig();
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  // Convert received bytes to a string
  char text[length + 1];
  snprintf(text, length + 1, "%s", payload);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(text);

  if (strcmp(topic, line1_topic) == 0)
  {
    snprintf(mqtt_line1, sizeof(mqtt_line1), "%s", text);
    need_redraw = true;
  }

  if (strcmp(topic, line2_topic) == 0)
  {
    snprintf(mqtt_line2, sizeof(mqtt_line2), "%s", text);
    need_redraw = true;
  }

  if (strcmp(topic, line3_topic) == 0)
  {
    snprintf(mqtt_line3, sizeof(mqtt_line3), "%s", text);
    need_redraw = true;
  }

  if (strcmp(topic, cmnd_temp_coefficient_topic) == 0)
  {
    temperatureCoef = atof(text);
    save_calibration();
  }

  if (strcmp(topic, cmnd_ds_temp_coefficient_topic) == 0)
  {
    dsTemperatureCoef = atof(text);
    save_calibration();
  }

  if (strcmp(topic, cmnd_temp_format) == 0)
  {
    processMessageScale(text);
  }

#ifdef OTA_UPGRADES
  if (strcmp(topic, cmnd_update_topic) == 0)
  {
    Serial.println("OTA request seen.\n");
    do_ota_upgrade(text);
    // Any OTA upgrade will stop the mqtt client, so if the
    // upgrade failed and we get here publishState() will fail.
    // Just return here, and we will reconnect from within the
    // loop().
    return;
  }
#endif

  publishState();
}

void calculateMachineId()
{
  MD5Builder md5;
  md5.begin();
  char chipId[25];
  sprintf(chipId, "%d", ESP.getChipId());
  md5.add(chipId);
  md5.calculate();
  md5.toString().toCharArray(machineId, sizeof(machineId));
}

void mqttReconnect()
{
  char clientId[18 + sizeof(machineId)];
  snprintf(clientId, sizeof(clientId), "Hauzman-thermometer-%s", machineId);

  // Loop until we're reconnected
  for (int attempt = 0; attempt < 3; ++attempt)
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (true == mqttClient.connect(clientId, username, password))
    {
      Serial.println("connected");

      // Subscribe to MQTT topics
      mqttClient.subscribe(line1_topic);
      mqttClient.subscribe(line2_topic);
      mqttClient.subscribe(line3_topic);
      mqttClient.subscribe(cmnd_temp_coefficient_topic);
      mqttClient.subscribe(cmnd_ds_temp_coefficient_topic);
      mqttClient.subscribe(cmnd_temp_format);
#ifdef OTA_UPGRADES
      mqttClient.subscribe(cmnd_update_topic);
#endif
      publishState();
      break;

    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

#ifdef HOME_ASSISTANT_DISCOVERY
bool publishSensorDiscovery(const char *config_key,
                            const char *device_class,
                            const char *name_suffix,
                            const char *state_topic,
                            const char *unit,
                            const char *value_template)
{
  static char topic[48 + sizeof(machineId)];

  snprintf(topic, sizeof(topic),
           "homeassistant/sensor/%s/%s/config", machineId, config_key);

  DynamicJsonDocument json(1024);
  json["device_class"] = device_class;
  json["name"] = String(ha_name) + " " + name_suffix;
  json["unique_id"] = String("Hauzman-") + machineId + "-" + config_key;
  json["state_topic"] = String(workgroup) + "/" + machineId + state_topic;
  json["unit_of_measurement"] = unit;
  json["value_template"] = value_template;

  json["device"]["identifiers"] = machineId;
  json["device"]["manufacturer"] = "";
  json["device"]["model"] = "Hauzman Thermometer";
  json["device"]["name"] = ha_name;
  json["device"]["sw_version"] = ESP.getSketchMD5();

  JsonArray connections = json["device"].createNestedArray("connections").createNestedArray();
  connections.add("mac");
  connections.add(WiFi.macAddress());

  Serial.print("Home Assistant discovery topic: ");
  Serial.println(topic);

  int payload_len = measureJson(json);
  if (!mqttClient.beginPublish(topic, payload_len, true))
  {
    Serial.println("beginPublish failed!\n");
    return false;
  }

  if (serializeJson(json, mqttClient) != payload_len)
  {
    Serial.println("writing payload: wrong size!\n");
    return false;
  }

  if (!mqttClient.endPublish())
  {
    Serial.println("endPublish failed!\n");
    return false;
  }

  return true;
}
#endif

void publishState()
{
  static char payload[80];
  snprintf(payload, sizeof(payload), "%f", temperatureCoef);
  mqttClient.publish(stat_temp_coefficient_topic, payload, true);
  snprintf(payload, sizeof(payload), "%f", dsTemperatureCoef);
  mqttClient.publish(stat_ds_temp_coefficient_topic, payload, true);

#ifdef HOME_ASSISTANT_DISCOVERY
  String homeAssistantTempScale = (true == configTempCelsius) ? "°C" : "°F";
  publishSensorDiscovery("temp",
                         "temperature",
                         "Temperature",
                         "/Temperature/temperature",
                         homeAssistantTempScale.c_str(),
                         "{{ value_json.temperature | round(1) }}");

  publishSensorDiscovery("humidity",
                         "humidity",
                         "Humidity",
                         "/Temperature/humidity",
                         "%",
                         "{{ value_json.humidity | round(0) }}");

  if (0 < sensors.getDeviceCount())
  {
    publishSensorDiscovery("DS18B20temp",
                           "temperature",
                           "DS18B20",
                           "/DS18B20/temperature",
                           "°C",
                           "{{ value_json.temperature }}");
  }
#endif
}

void publishSensorData(const char* subTopic, const char* key, const float value)
{
  StaticJsonDocument<100> json;
  json[key] = value;
  char payload[100];
  serializeJson(json, payload);
  char topic[200];
  sprintf(topic, "%s/%s/%s", workgroup, machineId, subTopic);
  mqttClient.publish(topic, payload, true);
}

void publishSensorData(const char* subTopic, const char* key, const String& value)
{
  StaticJsonDocument<100> json;
  json[key] = value;
  char payload[100];
  serializeJson(json, payload);
  char topic[200];
  sprintf(topic, "%s/%s/%s", workgroup, machineId, subTopic);
  mqttClient.publish(topic, payload, true);
}

bool isSensorAvailable(int sensorAddress)
{
  // Check if I2C sensor is present
  Wire.beginTransmission(sensorAddress);
  return 0 == Wire.endTransmission();
}

void handleHTU21D()
{
  // Check if temperature has changed
  const float tempTemperature = htu.readTemperature();
  if (1 <= abs(tempTemperature - sensorTemperature))
  {
    // Print new temprature value
    sensorTemperature = tempTemperature;
    Serial.print("Temperature: ");
    Serial.println(formatTemperature(sensorTemperature));

    // Publish new temperature value through MQTT
    publishSensorData("temperature", "temperature", convertTemperature(sensorTemperature));
  }

  // Check if humidity has changed
  const float tempHumidity = htu.readHumidity();
  if (1 <= abs(tempHumidity - sensorHumidity))
  {
    // Print new humidity value
    sensorHumidity = tempHumidity;
    Serial.print("Humidity: ");
    Serial.print(sensorHumidity);
    Serial.println("%");

    // Publish new humidity value through MQTT
    publishSensorData("humidity", "humidity", sensorHumidity);
  }
}

void sensorWriteData(int i2cAddress, uint8_t data)
{
  Wire.beginTransmission(i2cAddress);
  Wire.write(data);
  Wire.endTransmission();
}

void handleBH1750()
{
  //Wire.begin();
  // Power on sensor
  sensorWriteData(sensorBH1750, 0x01);
  // Set mode continuously high resolution mode
  sensorWriteData(sensorBH1750, 0x10);

  uint16_t tempAmbientLight;

  Wire.requestFrom(sensorBH1750, 2);
  tempAmbientLight = Wire.read();
  tempAmbientLight <<= 8;
  tempAmbientLight |= Wire.read();
  // s. page 7 of datasheet for calculation
  tempAmbientLight = tempAmbientLight / 1.2;

  if (1 <= abs(tempAmbientLight - sensorAmbientLight))
  {
    /*******************************************************************************************/
    // Print new brightness value
    sensorAmbientLight = tempAmbientLight;
    Serial.print("Light: ");
    Serial.print(tempAmbientLight);
    Serial.println("Lux");
    /*******************************************************************************************/
    // Publish new brightness value through MQTT
    publishSensorData("light", "light", sensorAmbientLight);
  }
}

void detectGesture()
{
  //read a gesture from the device
  const uint8_t gestureCode = apds.readGesture();
  // Skip if gesture has not been detected
  if (0 == gestureCode)
  {
    return;
  }
  String gesture = "";
  switch (gestureCode)
  {
    case APDS9960_DOWN:
      gesture = "down";
      break;
    case APDS9960_UP:
      gesture = "up";
      break;
    case APDS9960_LEFT:
      gesture = "left";
      break;
    case APDS9960_RIGHT:
      gesture = "right";
      break;
  }
  Serial.print("Gesture: ");
  Serial.println(gesture);
  // Publish the detected gesture through MQTT
  publishSensorData("gesture", "gesture", gesture);
}

void handleBMP()
{
  sensors_event_t event;
  bmp.getEvent(&event);
  if (!event.pressure)
  {
    // BMP180 sensor error
    return;
  }
  Serial.print("BMP180 Pressure: ");
  Serial.print(event.pressure);
  Serial.println(" hPa");
  float temperature;
  bmp.getTemperature(&temperature);
  Serial.print("BMP180 Temperature: ");
  Serial.println(formatTemperature(temperature));
  // For accurate results replace SENSORS_PRESSURE_SEALEVELHPA with the current SLP
  float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
  float altitude;
  altitude = bmp.pressureToAltitude(seaLevelPressure, event.pressure, temperature);
  Serial.print("BMP180 Altitude: ");
  Serial.print(altitude);
  Serial.println(" m");

  // Publish new pressure values through MQTT
  publishSensorData("BMPpressure", "BMPpressure", event.pressure);
  publishSensorData("BMPtemperature", "BMPtemperature", convertTemperature(temperature));
  publishSensorData("BMPaltitude", "BMPaltitude", altitude);
}

void handleSensors()
{
  if (isSensorAvailable(sensorHTU21D))
  {
    handleHTU21D();
  }
  if (isSensorAvailable(sensorBH1750))
  {
    handleBH1750();
  }
  if (isSensorAvailable(sensorBMP180))
  {
    handleBMP();
  }
}

float convertCelsiusToFahrenheit(float temperature)
{
  return (temperature * 9 / 5 + 32);
}

float convertTemperature(float temperature)
{
  return (true == configTempCelsius) ? temperature : convertCelsiusToFahrenheit(temperature);
}

String formatTemperature(float temperature)
{
  String unit = (true == configTempCelsius) ? "°C" : "°F";
  return String(convertTemperature(temperature), 1) + unit;
}

void setDefaultSensorLines()
{
  sensor_line1 = "Temp: " + formatTemperature(dhtTemperature);
  Serial.println(sensor_line1);
  sensor_line2 = "Humidity " + String(dhtHumidity, 0) + "%";
  Serial.println(sensor_line2);
  sensor_line3 = "";
}

void loop()
{
  // put your main code here, to run repeatedly:
  mqttClient.loop();

  // Reconnect if there is an issue with the MQTT connection
  const unsigned long mqttConnectionMillis = millis();
  if ( (false == mqttClient.connected()) && (mqttConnectionInterval <= (mqttConnectionMillis - mqttConnectionPreviousMillis)) )
  {
    mqttConnectionPreviousMillis = mqttConnectionMillis;
    mqttReconnect();
  }

  // Handle gestures at a shorter interval
  if (isSensorAvailable(APDS9960_ADDRESS))
  {
    detectGesture();
  }

  const unsigned long currentMillis = millis();
  if (sensorInterval <= (currentMillis - sensorPreviousMillis))
  {
    sensorPreviousMillis = currentMillis;
    handleSensors();

    // Read temperature and humidity from DHT22/AM2302
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

#ifdef ESP12_BLUE_LED_ALWAYS_ON
    pinMode(DHTPIN, OUTPUT);
    digitalWrite(DHTPIN, LOW);
#endif

    if (!isnan(humidity) && !isnan(temp))
    {
      // Adjust temperature depending on the calibration coefficient
      temp = temp * temperatureCoef;

      dhtTemperature = temp;
      dhtHumidity = humidity;
      publishSensorData("Temperature/temperature", "temperature", convertTemperature(temp));
      publishSensorData("Temperature/humidity", "humidity", humidity);

      // Calculate heat index
      float dhtHeatIndex = dht.computeHeatIndex(dhtTemperature, dhtHumidity, false);
      publishSensorData("Temperature/heatindex", "heatindex", convertTemperature(dhtHeatIndex));
      Serial.println("DHT Heat Index: " + formatTemperature(dhtHeatIndex));
    }
    setDefaultSensorLines();

    long softValue = WiFi.RSSI();
    String soft = "sw : " + String(VERSION) ;
    Serial.println(soft);
    if (0 < sensors.getDeviceCount())
    {
      sensors.requestTemperatures();
      float wtemp = sensors.getTempCByIndex(0);
      wtemp = wtemp * dsTemperatureCoef;
      dsTemperature = wtemp;
      publishSensorData("DS18B20/temperature", "temperature", convertTemperature(wtemp));
      sensor_line3 = "DS18B " + formatTemperature(dsTemperature);
      Serial.println(sensor_line3);
    }
    else
    {
      static int select = 0;
      switch (++select % 2) {
        case 0:
          timeClient.update();
          sensor_line3 = "      " + timeClient.getFormattedTime();
          break;
        default:
                   sensor_line3 = soft;
          break;
      }
    }

    need_redraw = true;

        publishSensorData("wifi/ssid", "ssid", WiFi.SSID());
        publishSensorData("wifi/bssid", "bssid", WiFi.BSSIDstr());
        publishSensorData("soft/soft", "soft", softValue);
        publishSensorData("wifi/ip", "ip", WiFi.localIP().toString());
        publishSensorData("sketch", "sketch", ESP.getSketchMD5());

#ifdef PUBLISH_CHIP_ID
    char chipid[9];
    snprintf(chipid, sizeof(chipid), "%08x", ESP.getChipId());
    publishSensorData("chipid", "chipid", chipid);
#endif
  }

  if (need_redraw)
  {
    drawDisplay(mqtt_line1[0] ? mqtt_line1 : sensor_line1.c_str(),
                mqtt_line2[0] ? mqtt_line2 : sensor_line2.c_str(),
                mqtt_line3[0] ? mqtt_line3 : sensor_line3.c_str());
    need_redraw = false;
  }

  // Press and hold the button to reset to factory defaults
  factoryReset();
}
