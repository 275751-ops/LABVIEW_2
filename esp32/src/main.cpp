//#include <Arduino.h>
// #include <Wire.h>
// #include <WiFi.h>
// #include "secrets.h"

// // void setup(){
// // Serial.begin(115200);
// // Serial.println("Start programu");

// // }



// // void loop(){
// // Serial.printf("costam\n\r");
// // }


// void connectWiFi() {
// Serial.print("Laczenie z Wi-Fi: ");
// Serial.println(WIFI_SSID);
// WiFi.mode(WIFI_STA);
// WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
// while (WiFi.status() != WL_CONNECTED) {
// delay(500);
// Serial.print(".");
// }
// Serial.println();
// Serial.println("Polaczono z Wi-Fi");
// Serial.print("Adres IP: ");
// Serial.println(WiFi.localIP());
// }

// void setup(){
//   Serial.begin(115200);
//   connectWiFi();
// }
// void loop(){}

// String generateDeviceIdFromEfuse() {
// uint64_t chipId = ESP.getEfuseMac();
// char id[32];
// snprintf(id, sizeof(id), "esp32-%04X%08X",
// (uint16_t)(chipId >> 32),
// (uint32_t)chipId);
// return String(id);
// }


#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
String deviceId;
String topic;
String generateDeviceIdFromEfuse() {
uint64_t chipId = ESP.getEfuseMac();
char id[32];
snprintf(id, sizeof(id), "esp32-%04X%08X",
(uint16_t)(chipId >> 32),
(uint32_t)chipId);
return String(id);
}
void connectWiFi() {
Serial.print("Laczenie z Wi-Fi: ");
Serial.println(WIFI_SSID);
WiFi.mode(WIFI_STA);
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
Serial.println();
Serial.println("Polaczono z Wi-Fi");
Serial.print("Adres IP: ");
Serial.println(WiFi.localIP());
}
void connectMQTT() {
mqttClient.setServer(MQTT_HOST, MQTT_PORT);
while (!mqttClient.connected()) {
Serial.print("Laczenie z MQTT...");
if (mqttClient.connect(deviceId.c_str())) {
Serial.println("OK");
} else {
Serial.print("blad, rc=");
Serial.print(mqttClient.state());
Serial.println(" - ponowna proba za 2 s");
delay(2000);
}
}
}
void publishMeasurement() {
StaticJsonDocument<256> doc;
doc["device_id"] = deviceId;
doc["sensor"] = "temperature";
doc["value"] = 24.5;
doc["unit"] = "C";
doc["ts_ms"] = millis();
char payload[256];
serializeJson(doc, payload);
mqttClient.publish(topic.c_str(), payload);
Serial.print("Publikacja na topic: ");
Serial.println(topic);
Serial.println(payload);
}
void setup() {
Serial.begin(115200);
delay(1000);
deviceId = generateDeviceIdFromEfuse();
topic = "lab/" + String(MQTT_GROUP) + "/" + deviceId + "/temperature";
Serial.print("Device ID: ");
Serial.println(deviceId);
connectWiFi();
connectMQTT();
}
void loop() {
if (WiFi.status() != WL_CONNECTED) {
connectWiFi();
}
if (!mqttClient.connected()) {
connectMQTT();
}
mqttClient.loop();
publishMeasurement();
delay(5000);
}