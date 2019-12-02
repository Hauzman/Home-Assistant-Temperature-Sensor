/*

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/******************************************************************************
 * NOTE: The settings here are the default settings for the first loading.  
 * After loading you will manage changes to the settings via the Web Interface.  
 * If you want to change settings again in the settings.h, you will need to 
 * erase the file system on the Wemos or use the “Reset Settings” option in 
 * the Web Interface.
 ******************************************************************************/
 
#include <ESP8266WiFi.h>              // -- > https://github.com/esp8266/Arduino
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>              // --> https://github.com/tzapu/WiFiManager 
#include <ArduinoOTA.h>
#include "FS.h"
#include <SPI.h>
#include <U8g2lib.h>                 // --> https://www.arduinolibraries.info/libraries/u8g2 ver.U8g2-2.23.18

//******************************
// Start Settings
//******************************

/*******************************************************************************************
Define Dht pin usin GPIO
*******************************************************************************************/

#define DHTPIN  0 // Pin D3 on wemos D1,
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

/*******************************************************************************************
Define DS18B20 pin using GPIO
*******************************************************************************************/

#define ONE_WIRE_BUS 2 // Pin D4 on wemos D1
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
Adafruit_APDS9960 apds;

/*******************************************************************************************
  For Oled display - here you can change the font for all line or separatly for each line
*******************************************************************************************/

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void drawDisplay(const char *line1, const char *line2 = "", const char *line3 = "")
{
// Write on OLED display
// Clear the internal memory
  u8g2.clearBuffer();
// Set appropriate font
  u8g2.setFont(u8g2_font_ncenR14_tr);
  u8g2.drawStr(0, 14, line1);
  u8g2.drawStr(0, 39, line2);
  u8g2.setFont(u8g2_font_fub11_tr);
  u8g2.drawStr(0, 64, line3);
// Transfer internal memory to the display
  u8g2.sendBuffer();
}

/******************************
 End Settings
******************************/
