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
#include <sys/time.h>
#include <time.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>


WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Obiekt czujnika BME280
Adafruit_BMP280 bme;

String deviceId;
String baseTopic; // Zmieniono na baseTopic, aby doklejać końcówki (temperature/humidity/pressure)

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
// void scanI2C() {
//   byte error, address;
//   int nDevices = 0;

//   Serial.println("Skanowanie I2C...");

//   for(address = 1; address < 127; address++) {
//     Wire.beginTransmission(address);
//     error = Wire.endTransmission();

//     if (error == 0) {
//       Serial.print("Znaleziono urządzenie pod adresem 0x");
//       Serial.println(address, HEX);
//       nDevices++;
//     }
//   }

//   if (nDevices == 0)
//     Serial.println("Brak urządzeń I2C");
// }


// Funkcja inicjalizująca BME280
void setupBME280() {
  Serial.println("Inicjalizacja BME280...");
  if (!bme.begin(0x76)) { // Zmień na 0x77 jeśli czujnik nie odpowiada
    Serial.println("Nie znaleziono czujnika BME280! Sprawdz polaczenia.");
    while (1); 
  }
  Serial.println("BME280 gotowy.");
}

// Zmodyfikowana funkcja wysyłająca JSON (teraz uniwersalna)
void publishMeasurement(long long ts_ms, String sensorType, float value, String unit) {
  // W ArduinoJson v7 używamy po prostu JsonDocument
  JsonDocument doc; 
  
  doc["schema_version"] = 1;
  doc["group_id"] = "g03";
  doc["device_id"] = deviceId;
  doc["sensor"] = sensorType;
  doc["value"] = value;
  doc["unit"] = unit;
  doc["ts_ms"] = ts_ms;

  char payload[256];
  serializeJson(doc, payload);

  // Tworzymy pełny topic, np. lab/g01/esp32-XXXX/temperature
  String currentTopic = baseTopic + "/" + sensorType;
  
  mqttClient.publish(currentTopic.c_str(), payload);
  
  Serial.print("Publikacja na topic: ");
  Serial.println(currentTopic);
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  //delay(250);
  
  deviceId = generateDeviceIdFromEfuse();
  // Zapisujemy bazowy topic bez końcówki
  baseTopic = "lab/" + String(MQTT_GROUP) + "/" + deviceId;
  
  Serial.print("Device ID: ");
  Serial.println(deviceId);
 // scanI2C();
  Wire.begin(21, 22);
  setupBME280();

connectWiFi();
connectMQTT();


  configTime(0, 0, "tempus1.gum.gov.pl", "tempus2.gum.gov.pl");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Oczekiwanie na synchronizacje czasu...");
    delay(500);
  }
  Serial.println("Czas zsynchronizowany.");
}

long long getTimestampMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((long long)tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  long long ts_ms = getTimestampMs();
  
  // 1. Odczyt danych z BME280
  float temp = bme.readTemperature();
  float ALT = bme.readAltitude(1013.25);
  float pres = bme.readPressure() / 100.0F;

  // 2. Wysłanie danych, jeśli odczyt jest poprawny (not NaN)
  if (!isnan(temp)) {
    publishMeasurement(ts_ms, "temperature", temp, "C");
  }
  if (!isnan(ALT)) {
    publishMeasurement(ts_ms, "Altitude", ALT, " m");
  }
  if (!isnan(pres)) {
    publishMeasurement(ts_ms, "pressure", pres, "hPa");
  }

  Serial.println("-------------------------");
  delay(5000);
}