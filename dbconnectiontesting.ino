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
const char* sasToken = "SharedAccessSignature sr=iotesp32dht22.azure-devices.net%2Fdevices%2Fesp32-dht22&sig=l7g5%2BAXQtfjv8hjAuzgZXrEKSlMiGW3IawxqQ6d3Sus%3D&se=1746004807"; // Update this when it expires

// ====== DHT22 Sensor Setup ======
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====== Clients ======
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  dht.begin();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  wifiClient.setInsecure(); // NOTE: Insecure. For testing only. Use certificate validation in production!

  mqttClient.setServer(mqttServer, mqttPort);
}

// ====== Connect to Azure IoT Hub ======
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
  if (!mqttClient.connected()) {
    connectToAzure();
  }

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["deviceId"] = deviceId; // ✅ Added deviceId for Cosmos DB

  char payload[256];
  serializeJson(doc, payload);

  String topic = "devices/" + String(deviceId) + "/messages/events/";

  if (mqttClient.publish(topic.c_str(), payload)) {
    Serial.println("Message sent to Azure IoT Hub:");
    Serial.println(payload);
  } else {
    Serial.println("Failed to send MQTT message.");
  }

  delay(10000); // Wait 10 seconds before sending next reading
}
