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
#include <WiFiClientSecure.h>



// WiFiClient espClient;
// PubSubClient mqttClient(espClient);

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
// Obiekt czujnika BME280
Adafruit_BMP280 bme;

String deviceId;
String baseTopic; // baseTopic, aby doklejać końcówki (temperature/humidity/pressure)

String generateDeviceIdFromEfuse() {
  uint64_t chipId = ESP.getEfuseMac();
  char id[32];
  snprintf(id, sizeof(id), "esp32-%04X%08X",
           (uint16_t)(chipId >> 32),
           (uint32_t)chipId);
  return String(id);
}

unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastMeasurementMs = 0;
const unsigned long WIFI_RETRY_MS = 5000;
const unsigned long MQTT_RETRY_MS = 3000;
const unsigned long MEASUREMENT_PERIOD_MS = 2000;

const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDMjCCAhqgAwIBAgIUb0q6H6gYDHIvqUhyyh5OY/VJ9YUwDQYJKoZIhvcNAQEL
BQAwMTELMAkGA1UEBhMCUEwxDDAKBgNVBAoMA1BXcjEUMBIGA1UEAwwLTW9qZVN1
cGVyQ0EwHhcNMjYwNjA4MTAyMjQxWhcNMjcwNjA4MTAyMjQxWjAxMQswCQYDVQQG
EwJQTDEMMAoGA1UECgwDUFdyMRQwEgYDVQQDDAtNb2plU3VwZXJDQTCCASIwDQYJ
KoZIhvcNAQEBBQADggEPADCCAQoCggEBAM/sm0+6UtL/v7NeBFuGQqTzoMK+G/tO
TlydObHzNRj4NzJJV3+HGBnD4q8FKR5BKQ8Wlx+e1n4kMAd7v6COHwmWEJ41jgPV
bz1mkdeQ+nrHAV8hwAKUVlBKhahzkmECbH/tL7blOMHPH+4K1m2/VEkW+f52uO8B
m+q/C1/m8vSEdRvk3qQbqiYwR345eh7+zonza6FYYk+hs/mB+T8rgD7OOSVqG1iH
h3A5fciBn+Hst3lb4xQu8Fkz+p/y/AHpXO3yM1leJfQ2gaXTYUQjheL6v2OybG/0
+L8EczqDguEQfA9R3nyXmwYRKXubakf1siVRCQdT4JvVxwMKNn0y7bUCAwEAAaNC
MEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0OBBYEFFjJ
xxWzQ757Ref7NQ2jrGocQYuzMA0GCSqGSIb3DQEBCwUAA4IBAQCl/rZyonxic6rm
KKVxq9JwBUyUrWKkdv2jhtosfTn8pzxgHNFWxaqBZjoGFmeIEICuJHx5EdABMN4O
AdPoiSUiOWkq5mcrcpsKtFZSDxn6xb4i7qH1MVCSkz4sWxhFN8bixNHCpvzCo4wy
UoNziwxm4OmpwPfS6JKJQrs0ScPCRHaK3AmngHB41vRsOaJgTp6MldeIk2+k/nj2
Ang8qSO0uzgcLBR9CAwc+6b75KWerWI1lrySd0QpAWANdkGKKQUQSF862xiHTAiQ
V9FY7MDuBLppeo8KHt0jORKaxFurjyrZ/YJaMPo6JQ1MLeuJyHMSgmMAuiIZ1VzB
mfy/2bNb
-----END CERTIFICATE-----
)EOF";


bool syncTime(uint32_t timeoutMs = 10000)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Brak WiFi - pomijam synchronizacje czasu");
        return false;
    }

    configTime(0, 0, "tempus1.gum.gov.pl", "tempus2.gum.gov.pl");

    struct tm timeinfo;
    uint32_t start = millis();

    while (millis() - start < timeoutMs)
    {
        if (getLocalTime(&timeinfo))
        {
            Serial.println("Czas zsynchronizowany");
            return true;
        }

        Serial.println("Oczekiwanie na synchronizacje czasu...");
        delay(500);
    }

    Serial.println("Timeout synchronizacji czasu");
    return false;
}


void connectWiFiIfNeeded() {
if (WiFi.status() == WL_CONNECTED) {
return;
}
if (millis() - lastWifiAttemptMs < WIFI_RETRY_MS) {
return;
}
lastWifiAttemptMs = millis();
Serial.println("WiFi disconnected. Trying reconnect...");
WiFi.disconnect();
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
if (WiFi.status() == WL_CONNECTED)
syncTime();
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


long long getTimestampMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((long long)tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
}

String statusTopic() {
return "lab/" + String(MQTT_GROUP) + "/" + deviceId + "/status";
}

void publishStatus(const char* state) {
  if (!mqttClient.connected()) return;
  JsonDocument doc; // Używamy nowszej wersji ArduinoJson
  doc["device_id"] = deviceId;
  doc["status"] = state;
  doc["ts_ms"] = getTimestampMs();
  char buffer[128];
  serializeJson(doc, buffer, sizeof(buffer));
  mqttClient.publish(statusTopic().c_str(), buffer, true); // Flaga retained: true [cite: 245]
}

bool connectMqttIfNeeded() {

if (WiFi.status() != WL_CONNECTED) {
return false;
}
if (mqttClient.connected()) {
return true;
}
if (millis() - lastMqttAttemptMs < MQTT_RETRY_MS) {
return false;
}
if (millis() - lastMqttAttemptMs < MQTT_RETRY_MS) {
return false;
}
lastMqttAttemptMs = millis();
String willPayload =
"{\"device_id\":\"" + deviceId + "\",\"status\":\"offline\"}";
bool ok = mqttClient.connect(
deviceId.c_str(),
statusTopic().c_str(),
0,
true,
willPayload.c_str()
);
if (ok) {
Serial.println("MQTT connected");
publishStatus("online");
} else {
Serial.print("MQTT connect failed, rc=");
Serial.println(mqttClient.state());
}
return ok;
}


// Funkcja inicjalizująca BME280
void setupBME280() {
  Serial.println("Inicjalizacja BME280...");
  if (!bme.begin(0x76)) { // Zmień na 0x77 jeśli czujnik nie odpowiada
    Serial.println("Nie znaleziono czujnika BME280! Sprawdz polaczenia.");
    while (1); 
  }
  Serial.println("BME280 gotowy.");
}

//funkcja wysyłająca JSON
void publishMeasurement(long long ts_ms, String sensorType, float value, String unit) {

  JsonDocument doc; 

  doc["schema_version"] = 1;
  doc["device_id"] = deviceId;
  doc["group_id"] = "g01";
  doc["sensor"] = sensorType;
  doc["value"] = value;
  doc["unit"] = unit;
  doc["ts_ms"] = ts_ms;

  char payload[256];
  serializeJson(doc, payload);

  // topic: lab/g01/esp32-XXXX/temperature
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
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
delay(500);
}

  //espClient.setInsecure();

  configTime(0, 0, "tempus1.gum.gov.pl", "tempus2.gum.gov.pl");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Oczekiwanie na synchronizacje czasu...");
    delay(500);
  }
  Serial.print("Czas zsynchronizowany. Aktualny rok: ");
  Serial.println(timeinfo.tm_year + 1900);

  espClient.setCACert(ca_cert);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
 // syncTime();
}




void loop() {
  connectWiFiIfNeeded();
  if (connectMqttIfNeeded()) {
    mqttClient.loop();
  

  // Publikacja pomiarów co określony czas (np. 2 sekundy)
    if (millis() - lastMeasurementMs >= MEASUREMENT_PERIOD_MS) {
      lastMeasurementMs = millis();
      
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
    }}
  delay(2000);
}
