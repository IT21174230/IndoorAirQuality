#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "DHT.h"

#define WIFI_SSID "Rain"
#define WIFI_PASSWORD "KsJ230492@"

#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Azure IoT Central Settings
const char* mqttServer = "global.azure-devices-provisioning.net";
const int mqttPort = 8883;

const char* idScope = "0ne00EA8005"; // From IoT Central
const char* deviceId = "esp32-dht22";
const char* sasToken = "SharedAccessSignature sr=0ne00EA8005/registrations/esp32-dht22&sig=KWBdz15iHNgEkuYsun02MN8026R7DGsfydU6GrFS6bo%3D&se=1745613645"; // From Python script

WiFiClientSecure secureClient;
PubSubClient client(secureClient);

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

void connectToMQTT() {
  secureClient.setInsecure(); // Skip cert validation (for now)

  client.setServer(mqttServer, mqttPort);

  String mqttClientId = String(deviceId);
  String username = String(idScope) + "/registrations/" + deviceId + "/api-version=2019-03-31";
  String password = sasToken;

  Serial.println("Connecting to MQTT...");

  while (!client.connected()) {
    if (client.connect(mqttClientId.c_str(), username.c_str(), password.c_str())) {
      Serial.println("\nConnected to Azure IoT Central via MQTT!");
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}

void sendTelemetry() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read DHT");
    return;
  }

  // Match the case-sensitive telemetry field names in IoT Central
  String payload = "{\"Temperature\":" + String(temperature) + ",\"Humidity\":" + String(humidity) + "}";
  Serial.println("Sending telemetry: " + payload);

  // Publishing to Azure DPS endpoint (this will later forward to assigned hub)
  client.publish(("devices/" + String(deviceId) + "/messages/events/").c_str(), payload.c_str());
}


void setup() {
  Serial.begin(115200);
  dht.begin();
  connectToWiFi();
  connectToMQTT();
}

void loop() {
  if (!client.connected()) {
    connectToMQTT(); // Reconnect if the MQTT connection is lost
  }

  sendTelemetry(); // Send telemetry data
  client.loop();   // Ensure the MQTT client processes incoming messages
  delay(10000);    // Send every 10 seconds
}
