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
const char* sasToken = "SharedAccessSignature sr=iotesp32dht22.azure-devices.net%2Fdevices%2Fesp32-dht22&sig=BL7nx9CgBbZUVBCM4Tvg6Uh76g23HEnyccHNfoZK3d4%3D&se=1746223732";

// ====== Azure Toggle ======
const bool ENABLE_AZURE = true;  // Set to true when ready

// ====== DHT22 Sensor Setup ======
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====== LED Setup ======
#define LED_PIN 25  // GPIO25 connected to LED through 330Ω resistor

// ====== Timing and Averaging Setup ======
const int MAX_SAMPLES = 5;
unsigned long sampleInterval = 10000;  // 10 seconds
float temperatureSamples[MAX_SAMPLES];
float humiditySamples[MAX_SAMPLES];
int sampleIndex = 0;
unsigned long lastSampleTime = 0;

// ====== Clients ======
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// ====== Average Calculation ======
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

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // LED OFF at boot

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  if (ENABLE_AZURE) {
    wifiClient.setInsecure();
    mqttClient.setServer(mqttServer, mqttPort);
  }
}

// ====== Connect to Azure ======
void connectToAzure() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to Azure IoT Hub...");
    String username = String(mqttServer) + "/" + deviceId + "/?api-version=2020-09-30";
    if (mqttClient.connect(deviceId, username.c_str(), sasToken)) {
      Serial.println(" Connected!");
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Retrying in 5s...");
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

    // ====== Store in buffer for averaging ======
    temperatureSamples[sampleIndex] = temperature;
    humiditySamples[sampleIndex] = humidity;
    sampleIndex++;

    // ====== Send raw data to Azure ======
    StaticJsonDocument<200> doc;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["deviceId"] = deviceId;

    char payload[256];
    serializeJson(doc, payload);
    String topic = "devices/" + String(deviceId) + "/messages/events/";

    Serial.print("Raw Data - Temp: ");
    Serial.print(temperature);
    Serial.print(" °C, Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    if (ENABLE_AZURE) {
      if (mqttClient.publish(topic.c_str(), payload)) {
        Serial.println("Sent raw data to Azure.");
      } else {
        Serial.println("Failed to send MQTT message.");
      }
    } else {
      Serial.println("Azure disabled - Data not sent.");
      Serial.println(payload);
    }

    // ====== LED control every 5 samples ======
    if (sampleIndex >= MAX_SAMPLES) {
      float avgTemp = calculateAverage(temperatureSamples, MAX_SAMPLES);
      float avgHum = calculateAverage(humiditySamples, MAX_SAMPLES);

      Serial.print("Average Humidity for LED: ");
      Serial.println(avgHum);

      if (avgHum <= 40.0) {
        digitalWrite(LED_PIN, HIGH);  // LED ON
        Serial.println("LED ON - High humidity (>= 50%).");
      } else {
        digitalWrite(LED_PIN, LOW);   // LED OFF
        Serial.println("LED OFF - Humidity normal.");
      }

      sampleIndex = 0;  // Reset sample buffer
    }
  }
}
