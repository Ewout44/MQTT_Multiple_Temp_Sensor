# MQTT_Multiple_Temp_Sensor

This code reads values from a set of one or more Dallas One Wire DS18B20 temperature
sensors connected to an ESP8266 board, and publishes the readings using MQTT.
Borrows from various examples by Adafruit, Dallas Temperature and Arduino libraries.

The sensors used were Adafruit Waterproof DS18B20 Digital temperature sensor + extras.
The ESP8266 module was an Adafruit HUZZAH ESP8266 breakout.

The configuration of the WiFi connection and the MQTT server are
setup by connecting to an access point the ESP8266 creates on first
startup or when forced by holding the flash button for 5 seconds or so then releasing it.
More details on how this works are
documented in the example code for the WiFIManager library at:
https://github.com/tzapu/WiFiManager

