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
MIID9zCCAt+gAwIBAgIUKgv86JcZxvVJcc5y0DVOlQKjA9QwDQYJKoZIhvcNAQEL
BQAwgYoxCzAJBgNVBAYTAlBMMRUwEwYDVQQIDAxEb2xub3NsYXNraWUxEDAOBgNV
BAcMB1dyb2NsYXcxDDAKBgNVBAoMA3B3cjEMMAoGA1UECwwDUFdyMRMwEQYDVQQD
DApwb3ByemVjem55MSEwHwYJKoZIhvcNAQkBFhJuaWUgbm8gdHlsZSB0byBuaWUw
HhcNMjYwNTI1MTU1NjE3WhcNMzYwNTIyMTU1NjE3WjCBijELMAkGA1UEBhMCUEwx
FTATBgNVBAgMDERvbG5vc2xhc2tpZTEQMA4GA1UEBwwHV3JvY2xhdzEMMAoGA1UE
CgwDcHdyMQwwCgYDVQQLDANQV3IxEzARBgNVBAMMCnBvcHJ6ZWN6bnkxITAfBgkq
hkiG9w0BCQEWEm5pZSBubyB0eWxlIHRvIG5pZTCCASIwDQYJKoZIhvcNAQEBBQAD
ggEPADCCAQoCggEBALDWE9uXUrtDUcnKEbDewuugqK0B4TaimYynYRHTmbDpHg1M
RAIlGyKLbMQZhYplK0/VH0jyqb2+rgj40NRhUEi6RPj+p8AUM+zUW/IG/+5ES2al
WH0pU75eGx5l6tFJ8UxMVs2U+eqgtJN7N3p9TM4biVk+Z8z6kFAZUthFCe5aT15D
EwMMx7FPXku6Bb5kGVfWo8cfpv2nCQjia+1Wk7nUU9YUQ9f/jyLuRPsP2u/K/Hum
0YeOI/+T8gQ2Pg62pErV1+aghzHG4j5KiWxY9CSDv3WsGdiZ67QigZQ/om7f2kgv
fCMC5LMGv20WBttCMYYNwKoJHnMny0VhApdcBO8CAwEAAaNTMFEwHQYDVR0OBBYE
FLj4RMFT2N19m9lCBJ+4vfPFR588MB8GA1UdIwQYMBaAFLj4RMFT2N19m9lCBJ+4
vfPFR588MA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBAFdwjx0D
ZWQhSHhD5hwQnw6yVpMMLLoxjw3jndtq/VghzkGVYvz/hGmHVqTqouo4f6dVHpv3
onuPjIMGz/h6I4CY9xTBnOS4kAzgl7YNo1EtJcCMUGOgGM6mbHq2LoXPIiz+GFdX
vMdAmGONkqA84ifzcV8Rh1MH5InUsKkEWMwf80wkxZZ1BqfVCr1KRcXIKCBDYlM6
G4JwroXCkM4i1K3LM5EFlHAJmHOdPOwPYqVvqUyaV8hFxCA8IpdUeZE2fBdfXn2R
cR9yPx+WtJAjoxLXlMNfZSBnwUmkMnZkmWxWeaqfyeBjYzhm46mhvlz0vyBiT2hh
teNecXzAPG1CwHg=
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
if (mqttClient.connect("esp32_client")) {
mqttClient.publish("test/topic", "Polaczenie TLS dziala poprawnie");
}

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  espClient.setCACert(ca_cert);

  configTime(0, 0, "tempus1.gum.gov.pl", "tempus2.gum.gov.pl");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Oczekiwanie na synchronizacje czasu...");
    delay(500);
  }
  Serial.println("Czas zsynchronizowany.");
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