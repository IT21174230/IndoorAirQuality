#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ====== WiFi Config ======
const char* ssid = "SLT_FIBRE";
const char* password = "srisha2830";

// ====== Azure IoT Hub Settings ======
const char* mqttServer = "iotesp32dht22.azure-devices.net";
const int mqttPort = 8883;
const char* deviceId = "esp32-dht22";
const char* sasToken = "SharedAccessSignature sr=iotesp32dht22.azure-devices.net%2Fdevices%2Fesp32-dht22&sig=l7g5%2BAXQtfjv8hjAuzgZXrEKSlMiGW3IawxqQ6d3Sus%3D&se=1746004807"; // Update when expired

// ====== Azure Toggle ======
const bool ENABLE_AZURE = false;  // Set true when ready to connect to Azure

// ====== DHT22 Sensor Setup ======
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====== Relay Setup ======
#define RELAY_PIN 25

// ====== Averaging Setup ======
const int MAX_SAMPLES = 5;
unsigned long sampleInterval = 10000;  // 10 seconds
float temperatureSamples[MAX_SAMPLES];
float humiditySamples[MAX_SAMPLES];
int sampleIndex = 0;
unsigned long lastSampleTime = 0;

// ====== Clients ======
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// ====== Average Function ======
float calculateAverage(float* samples, int count) {
  float sum = 0;
  for (int i = 0; i < count; i++) {
    sum += samples[i];
  }
  return (count > 0) ? (sum / count) : 0;
}

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Start with diffuser OFF

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  if (ENABLE_AZURE) {
    wifiClient.setInsecure(); // For testing only
    mqttClient.setServer(mqttServer, mqttPort);
  }
}

// ====== Connect to Azure ======
void connectToAzure() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to Azure IoT Hub...");
    String username = String(mqttServer) + "/" + deviceId + "/?api-version=2020-09-30";
    if (mqttClient.connect(deviceId, username.c_str(), sasToken)) {
      Serial.println(" Connected to Azure!");
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Trying again in 5 seconds...");
      delay(5000);
    }
  }
}

// ====== Main Loop ======
void loop() {
  if (ENABLE_AZURE && !mqttClient.connected()) {
    connectToAzure();
  }
  if (ENABLE_AZURE) {
    mqttClient.loop();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentMillis;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    temperatureSamples[sampleIndex] = temperature;
    humiditySamples[sampleIndex] = humidity;
    sampleIndex++;

    Serial.print("Sample ");
    Serial.print(sampleIndex);
    Serial.print(" - Temp: ");
    Serial.print(temperature);
    Serial.print(" °C, Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    if (sampleIndex >= MAX_SAMPLES) {
      float avgTemp = calculateAverage(temperatureSamples, MAX_SAMPLES);
      float avgHum = calculateAverage(humiditySamples, MAX_SAMPLES);

      // === Relay Control Logic ===
      if (avgHum < 40.0) {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("Relay ON - Diffuser ON (low humidity).");
      } else if (avgHum >= 55.0) {
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Relay OFF - Diffuser OFF (humidity normal).");
      } else {
        Serial.println("Humidity in normal range - No change to relay.");
      }

      // === Create Payload ===
      StaticJsonDocument<200> doc;
      doc["avg_temperature"] = avgTemp;
      doc["avg_humidity"] = avgHum;
      doc["deviceId"] = deviceId;

      char payload[256];
      serializeJson(doc, payload);
      String topic = "devices/" + String(deviceId) + "/messages/events/";

      // === Send to Azure or Print ===
      if (ENABLE_AZURE) {
        if (mqttClient.publish(topic.c_str(), payload)) {
          Serial.println("Sent average data to Azure:");
          Serial.println(payload);
        } else {
          Serial.println("Failed to send MQTT message.");
        }
      } else {
        Serial.println("Azure disabled - Data not sent.");
        Serial.println(payload);
      }

      sampleIndex = 0;
    }
  }
}
