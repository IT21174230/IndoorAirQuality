#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ====== WiFi Config ======
const char* ssid = "Rain";
const char* password = "KsJ230492@";

// ====== Azure IoT Hub Device Connection String ======
const char* connectionString = "HostName=iotesp32dht22.azure-devices.net;DeviceId=esp32-dht22;SharedAccessKey=edND3k1l5eh8t2yO4jGRQCijaYUOyAdEvF/6bSUeGCY=";

// ====== DHT22 Sensor Setup ======
#define DHTPIN 4       // GPIO pin connected to DHT22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====== MQTT Settings ======
const char* mqttServer = "iotesp32dht22.azure-devices.net";
const int mqttPort = 8883;
const char* mqttClientId = "esp32-dht22";
const char* mqttUsername = "iotesp32dht22.azure-devices.net/esp32-dht22/?api-version=2020-09-30";

// ====== Generate SAS Token Separately ======
// For now, you'll manually paste a SAS token here (expires in 1 hour by default)
const char* mqttPassword = "SharedAccessSignature sr=iotesp32dht22.azure-devices.net%2Fdevices%2Fesp32-dht22&sig=4OU%2BWkRQt%2BEjdxIVsJa78XQCCU9kpmS9q86mqSBinro%3D&se=1745649486";

// ====== Secure Client and MQTT Client ======
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// ====== Setup Function ======
void setup() {
  Serial.begin(115200);
  dht.begin();

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");

  // Configure secure client
  wifiClient.setInsecure(); // Skip cert verification (for testing only!)

  mqttClient.setServer(mqttServer, mqttPort);
}

// ====== Connect to MQTT (Azure IoT Hub) ======
void connectToAzure() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to Azure IoT Hub...");
    if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
      Serial.println("Connected to Azure!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Trying again in 5 seconds...");
      delay(5000);
    }
  }
}

// ====== Loop Function ======
void loop() {
  if (!mqttClient.connected()) {
    connectToAzure();
  }

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Build JSON payload
  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;

  char payload[256];
  serializeJson(doc, payload);

  // MQTT Topic
  String topic = "devices/" + String(mqttClientId) + "/messages/events/";

  // Publish
  if (mqttClient.publish(topic.c_str(), payload)) {
    Serial.println("Message sent to Azure:");
    Serial.println(payload);
  } else {
    Serial.println("Failed to send message.");
  }

  delay(10000); // Send data every 10 seconds
}
