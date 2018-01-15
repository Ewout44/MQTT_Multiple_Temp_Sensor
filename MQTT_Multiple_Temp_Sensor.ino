/*
 * This code reads values from a set of one or more DS18B20 Dallas One Wire temperature
 * sensors with an ESP8266 board, and publishes the readings using MQTT.
 * Borrows from various examples by Adafruit, Dallas One Wire and Arduino libraries.
 *
 */

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
// Include the sensor libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>
// for WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define TRIGGER_PIN 0 // Forces WiFiManager to reset and ask for new setup

#define deviceName "sensor"

#define red_led 0
#define blue_led 2

//define your default values here, if there are different values in config.json, they are overwritten.
char client_name[40] = "ESP8266Client_multitemp1_sensor";
char mqtt_server[40] = "";
char mqtt_port[6] = "1883";
char mqtt_user[40] = "";
char mqtt_password[40] = "";
char device_set[10] = "1";
char temp_corr[20] = "0";

// count of devices
int devices = 0;
// pointer to array we will define later to hold device addresses
DeviceAddress * deviceAddresses;


//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

WiFiClient espClient;
PubSubClient client(espClient);

// Data wire is plugged into port 12 on the ESP8266
#define ONE_WIRE_BUS 12
#define TEMPERATURE_PRECISION 9

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to convert a device address to string
String addressToString(DeviceAddress deviceAddress)
{
  String address;
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) address.concat("0");
    String addrByteHex = String(deviceAddress[i], HEX);
    address.concat(addrByteHex);
    address.toUpperCase();
  }
  return address;
}

void setup() {
  // Setup the two LED ports to use for signaling status
  pinMode(red_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  digitalWrite(red_led, HIGH);  
  digitalWrite(blue_led, HIGH);  
  Serial.begin(115200);
  // If the trigger pin is held during setup we will force a WiFi configuration
  // setup instead of using the stored values
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    setup_wifi(true);
  } else {
    setup_wifi(false);
  }

  // Start sensor
  sensors.begin();

  // locate devices on the bus
  devices = sensors.getDeviceCount();
  
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(devices, DEC);
  Serial.println(" devices.");
  // This dynamically allocates the deviceAddresses array to the number of devices
  // We already created the pointer to it above so it's global
  deviceAddresses = new DeviceAddress [devices];
  
  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

// Search for the number of devices we found on the bus and set each ID
  // First reset the search
  oneWire.reset_search();
  // If the bus is shorted or other problems we will see no devices
  int i = 0;
  for (i=0;i<devices;i++) {
    if (!oneWire.search(deviceAddresses[i])) {
      Serial.println("Unable to find address for device " + i);
    } else {
      // Print the address we found
      Serial.print("Device ");
      Serial.print(String(i));
      Serial.print(" Address: ");
      printAddress(deviceAddresses[i]);
      // Set the resolution
      sensors.setResolution(deviceAddresses[i], TEMPERATURE_PRECISION);
      // Print the reported resolution for confirmation
      Serial.print("Device ");
      Serial.print(String(i));
      Serial.print(" Resolution: ");
      Serial.println(sensors.getResolution(deviceAddresses[i]));
    }
  }

}

void blink_red() {
  digitalWrite(red_led, LOW);
  delay(20);
  digitalWrite(red_led, HIGH);  
}

void blink_blue() {
  digitalWrite(blue_led, LOW);
  delay(20);
  digitalWrite(blue_led, HIGH);  
}

void setup_wifi(bool force) {
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.prettyPrintTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(client_name, json["client_name"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(device_set, json["device_set"]);
          strcpy(temp_corr, json["temp_corr"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_client_name("client", "client name", client_name, 40);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 40);
  WiFiManagerParameter custom_device_set("set", "device set", device_set, 10);
  WiFiManagerParameter custom_temp_corr("tempcorr", "temp C correction", temp_corr, 20);
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_client_name);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_device_set);
  wifiManager.addParameter(&custom_temp_corr);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //it starts an access point with the specified name
  //here  "OnDemandAP"
  //and goes into a blocking loop awaiting configuration

  if (force) {
    // Force to start the configuration AP
    //WITHOUT THIS THE AP DOES NOT SEEM TO WORK PROPERLY WITH SDK 1.5 , update to at least 1.5.1
    WiFi.mode(WIFI_STA);
  
    if (!wifiManager.startConfigPortal("OnDemandAP")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  } else {
    // Connect with saved settings, only start the configuration AP if connection fails
    wifiManager.autoConnect("OnDemandAP");
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  
  //read updated parameters
  strcpy(client_name, custom_client_name.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(device_set, custom_device_set.getValue());
  strcpy(temp_corr, custom_temp_corr.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["client_name"] = client_name;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["device_set"] = device_set;
    json["temp_corr"] = temp_corr;   

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.print("local ip ");
  Serial.println(WiFi.localIP());

}

void reconnect() {
  // Loop until we're reconnected
  // Server or port may have been updated
  Serial.println(mqtt_port);
  int mqtt_port_int = String(mqtt_port).toInt();
  Serial.println(mqtt_port_int);
  client.setServer(mqtt_server, mqtt_port_int);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect(client_name)) {
    if (client.connect(client_name, mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      blink_red();
      delay(200);
      blink_red();
      delay(4760);
      // If the trigger pin is held this will get us out of a MQTT reconnect loop
      // to set new MQTT information in case the WiFi connection info is good
      // but the MQTT setup needs to be changed
      if ( digitalRead(TRIGGER_PIN) == LOW ) {
        setup_wifi(true);
      }
    }
  }
}

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}

long lastMsg = 0;
long lastForceMsg = 0;
bool forceMsg = false;
float temp[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
float diff = 1.0;


void loop() {
  // If the trigger pin is held we'll force a new WiFi and MQTT setup cycle
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    setup_wifi(true);
  }
  
  if (! client.loop()) { // run the client loop and if it fails try to reconnect
    Serial.println("Not connected to MQTT....");
    reconnect();
    delay(5000);
  }

  // In case we have a sensor with temperature offset inaccuracy
  // we'll add a fixed correction offset to the temp in deg C and
  // let the conversion to deg F take care of itself
  float temp_corr_c=String(temp_corr).toFloat();

  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;

    // MQTT broker could go away and come back at any time
    // so doing a forced publish to make sure something shows up
    // within the first 5 minutes after a reset
    if (now - lastForceMsg > 300000) {
      lastForceMsg = now;
      forceMsg = true;
      Serial.println("Forcing publish every 5 minutes...");
    }

    // Request to all devices on the bus
    sensors.requestTemperatures();
    int i = 0;
    for (i=0;i<devices;i++) {
      float newTemp = sensors.getTempC(deviceAddresses[i]);
      if (checkBound(newTemp, temp[i], diff) || forceMsg) {
        temp[i] = newTemp;
        float temp_c=temp[i]+temp_corr_c; // Celsius
        float temp_f=temp[i]*1.8F+32.0F; // Fahrenheit
        Serial.print("New temperature of ");
        printAddress(deviceAddresses[i]);
        Serial.print(": ");
        Serial.print(String(temp_c) + " degC   ");
        Serial.println(String(temp_f) + " degF");
        String temperature_c_topic = deviceName;
        temperature_c_topic.concat("/");
        temperature_c_topic.concat(String(device_set));
        temperature_c_topic.concat("/");
        temperature_c_topic.concat(addressToString(deviceAddresses[i]));
        temperature_c_topic.concat("/temperature/degreeCelsius");
        Serial.print("Publishing to topic ");
        Serial.println(temperature_c_topic);
        char temperature_c_topic_buf[temperature_c_topic.length()+1];
        temperature_c_topic.toCharArray(temperature_c_topic_buf, temperature_c_topic.length()+1);
        client.publish(temperature_c_topic_buf, String(temp_c).c_str(), true);
        
        String temperature_f_topic = deviceName;
        temperature_f_topic.concat("/");
        temperature_f_topic.concat(String(device_set));
        temperature_f_topic.concat("/");
        temperature_f_topic.concat(addressToString(deviceAddresses[i]));
        temperature_f_topic.concat("/temperature/degreeFahrenheit");
        Serial.print("Publishing to topic ");
        Serial.println(temperature_f_topic);
        char temperature_f_topic_buf[temperature_f_topic.length()+1];
        temperature_f_topic.toCharArray(temperature_f_topic_buf, temperature_f_topic.length()+1);
        client.publish(temperature_f_topic_buf, String(temp_f).c_str(), true);
        blink_blue();
      }
    }
    forceMsg = false;
  }
}
