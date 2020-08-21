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
 
#include <PubSubClient.h>          // -- > https://github.com/knolleary/pubsubclient
#define OTA_UPGRADES 1
#define HOME_ASSISTANT_DISCOVERY 1

/******************************
 Start Settings
******************************/
//#define OTA_SERVER "192.168.43.250"
// You perform an OTA upgrade by publishing a MQTT command, like
// this:
//
// mosquitto_pub -h mqtt-server.example.com \
//     -t cmnd/$MACHINEID/update \
//     -m '{"file": "/Hauzman.bin", "server": "192.168.1.110", "port": 8080 }'
//
// The port defaults to 80.

/******************************
 MQTT setup
******************************/
//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "192.168.1.110";
char mqtt_port[6] = "1883";
char workgroup[32] = "workgroup";

/******************************
 MQTT username and password
******************************/
char username[20] = "mqtt";          // Here you put the username what you define in the HomeAssistant MQTT 
char password[20] = "Bubulina1";          // Here you put the password what you define in the HomeAssistant MQTT 
#ifdef HOME_ASSISTANT_DISCOVERY
char ha_name[32+1] = "";         // Make sure the machineId fits.for example "homeassistant"
#endif


/*******************************************************************************************/
// MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);
   
//******************************
// End Settings
//******************************
