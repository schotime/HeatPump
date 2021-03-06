
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <HeatPump.h>

#include "mitsubishi_heatpump_mqtt_esp8266.h"

// wifi, mqtt and heatpump client instances
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
HeatPump hp;

// debug mode, when true, will send all packets received from the heatpump to topic heatpump_debug_topic
// this can also be set by sending "on" to heatpump_debug_set_topic
bool _debugMode = true;

void setup() {
  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, HIGH);
  pinMode(blueLedPin, OUTPUT);
  digitalWrite(blueLedPin, HIGH);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    // wait 500ms, flashing the blue LED to indicate WiFi connecting...
    digitalWrite(blueLedPin, LOW);
    delay(250);
    digitalWrite(blueLedPin, HIGH);
    delay(250);
  }

  // startup mqtt connection
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  mqttConnect();

  // connect to the heatpump. Callbacks first so that the hpPacketDebug callback is available for connect()
  hp.setSettingsChangedCallback(hpSettingsChanged);
  hp.setRoomTempChangedCallback(sendCurrentRoomTemperature);
  hp.setPacketCallback(hpPacketDebug);
  hp.connect(&Serial);
}

void hpSettingsChanged() {
    const size_t bufferSize = JSON_OBJECT_SIZE(6);
    DynamicJsonBuffer jsonBuffer(bufferSize);
    
    JsonObject& root = jsonBuffer.createObject();
  
    heatpumpSettings currentSettings = hp.getSettings();
    
    root["power"]       = currentSettings.power;
    root["mode"]        = currentSettings.mode;
    root["temperature"] = currentSettings.temperature;
    root["fan"]         = currentSettings.fan;
    root["vane"]        = currentSettings.vane;
    root["wideVane"]    = currentSettings.wideVane;

    char buffer[512];
    root.printTo(buffer, sizeof(buffer));

    bool retain = true;
    mqtt_client.publish(heatpump_topic, buffer, retain);
}

void sendCurrentRoomTemperature(unsigned int currentRoomTemperature) {
    const size_t bufferSize = JSON_OBJECT_SIZE(1);
    DynamicJsonBuffer jsonBuffer(bufferSize);
    
    JsonObject& root = jsonBuffer.createObject();
  
    root["roomTemperature"] = currentRoomTemperature;

    char buffer[512];
    root.printTo(buffer, sizeof(buffer));

    bool retain = true;
    mqtt_client.publish(heatpump_temperature_topic, buffer, retain);
}

void hpPacketDebug(byte* packet, unsigned int length, char* packetDirection) {
  if(_debugMode) {
    String message;
    for (int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0"; // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }
  
    const size_t bufferSize = JSON_OBJECT_SIZE(1);
    DynamicJsonBuffer jsonBuffer(bufferSize);
    
    JsonObject& root = jsonBuffer.createObject();
  
    root[packetDirection] = message;
  
    char buffer[512];
    root.printTo(buffer, sizeof(buffer));
    
    mqtt_client.publish(heatpump_debug_topic, buffer);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Copy payload into message buffer
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
    
  if(strcmp(topic, heatpump_set_topic) == 0) { //if the incoming message is on the heatpump_set_topic topic...
    // Parse message into JSON
    const size_t bufferSize = JSON_OBJECT_SIZE(6);
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root = jsonBuffer.parseObject(message);
  
    if (!root.success()) {
      mqtt_client.publish(heatpump_debug_topic, "!root.success(): invalid JSON on heatpump_set_topic...");
      return;
    }
  
    // Step 3: Retrieve the values
    if (root.containsKey("power")) {
      String power = root["power"];
      hp.setPowerSetting(power);
    }
    
    if (root.containsKey("mode")) {
      String mode = root["mode"];
      hp.setModeSetting(mode);
    }
    
    if (root.containsKey("temperature")) {
      int temperature = root["temperature"];
      hp.setTemperature(temperature);
    }
    
    if (root.containsKey("fan")) {
      String fan = root["fan"];
      hp.setFanSpeed(fan);
    }
    
    if (root.containsKey("vane")) {
      String vane = root["vane"];
      hp.setVaneSetting(vane);
    }
    
    if (root.containsKey("wideVane")) {
      String wideVane = root["wideVane"];
      hp.setWideVaneSetting(wideVane);
    }
    
    bool result = hp.update();
  
    if(!result) {
      mqtt_client.publish(heatpump_debug_topic, "heatpump: update() failed");
    }
  } else if(strcmp(topic, heatpump_debug_set_topic) == 0) { //if the incoming message is on the heatpump_debug_set_topic topic...
    if(strcmp(message, "on") == 0) {
      _debugMode = true;
      mqtt_client.publish(heatpump_debug_topic, "debug mode enabled");
    } else if(strcmp(message, "off") == 0) {
      _debugMode = false;
      mqtt_client.publish(heatpump_debug_topic, "debug mode disabled");
    }
  } else {
    mqtt_client.publish(heatpump_debug_topic, strcat("heatpump: wrong mqtt topic: ", topic));
  }
}

void mqttConnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    // Attempt to connect
    if (mqtt_client.connect(client_id, mqtt_username, mqtt_password)) {
      mqtt_client.subscribe(heatpump_set_topic);
      mqtt_client.subscribe(heatpump_debug_set_topic);
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


uint lastTempSend = 0;

void loop() {
  if (!mqtt_client.connected()) {
    mqttConnect();
  }

  hp.sync();
  
  if(!lastTempSend || millis() > (lastTempSend + SEND_ROOM_TEMP_INTERVAL_MS)) { // only send the temperature every 60s
    sendCurrentRoomTemperature(hp.getRoomTemperature());
    lastTempSend = millis();
  }

  mqtt_client.loop();
}



