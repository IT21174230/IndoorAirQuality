#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>  

// Wi-Fi credentials
const char* ssid = "Rain";
const char* password = "KsJ230492@";

// Azure IoT Hub settings
const char* iotHubName = "IEMDevice"; // no .azure-devices.net
const char* deviceId = "IEM_DEVICE_Air";
const char* sasToken = "SharedAccessSignature sr=IEMDevice.azure-devices.net%2Fdevices%2FIEM_DEVICE_Air&sig=CApemGLly57Iw72%2FPd7CDAOLz6a2FM1rfqlP1Djz%2BJ0%3D&se=1745669299";  // Replace this

// Azure endpoint and port
const char* mqttServer = "IEMDevice.azure-devices.net";  // Resolved IP (use domain in production)
const int mqttPort = 8883;

// MQTT connection details
String clientId = deviceId;
String username = String(iotHubName) + ".azure-devices.net/" + deviceId + "/?api-version=2023-06-30";

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// DigiCert Global Root G2 (Valid for Azure IoT)
const char* azure_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQDrr9xt64P3Ky/NXLhP1PtjANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMR8wHQYDVQQDExZEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0xMzAxMDEwMDAwMDBaFw0zMzAxMDEwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xHzAdBgNVBAMTFkRpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEArfLNz9t1gzmz1M0aDQLS23/ie0u/Ayz9u/5u\n" \
"Y5Z0XjNu8lHnEj6QO6AYtM3lbm+59MTYF3pjWfL/9l3VFNwryLJSt/ivcgaTUNMX\n" \
"z7XbL1IXUdo+1xNwOEI91eoaMdYg1zKz8xUbXQjKH0KMTzA0+f7XSP5aXoaZySUw\n" \
"DUoXk97Ze8Cl6bnF7yYVMpNGnGP2HTC1I44N2ZsPU9FeI/rWcPHDG9zzSwOFOIyu\n" \
"TUAEx+cN8gKuw/OCpWmG9pBErpwi+Qks8u1ZfA+a5kxwN4FZJ9S5V9KgR4CSkxR6\n" \
"tNnn6VE1wK4qlrzytVnB+PIXLmjyP+eA0T3+rYG1tW3y4dP+lwIDAQABo0IwQDAd\n" \
"BgNVHQ4EFgQUN6/i+5GZ5UiGTDytwNWoUP8zNPgwDwYDVR0TAQH/BAUwAwEB/zAK\n" \
"BggqhkjOPQQDAgNHADBEAiATl8wKDkWJjzZQz1zYd8G5RBF3MP+XXE0StgfcHxCE\n" \
"LgIgJS2+xhp9S0X8PfFMUqOEED0h4KBY0Drh4g+uEN3lSm0=\n" \
"-----END CERTIFICATE-----\n";

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected!");
}

void connectToAzureMQTT() {
  mqttClient.setServer(mqttServer, mqttPort);
  wifiClient.setCACert(azure_root_ca);
  wifiClient.setHandshakeTimeout(30);  // Optional, increases TLS timeout

  Serial.println("Connecting to Azure IoT Hub...");

  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str(), username.c_str(), sasToken)) {
      Serial.println("✅ Connected to Azure IoT Hub!");
    } else {
      Serial.print("❌ Failed. State: ");
      Serial.println(mqttClient.state());
      delay(10000);  // wait 10 seconds before retry
    }
  }
}
void publishSensorData() {
  float lpg = random(20, 100);     // Simulated MQ2 data
  float co = random(10, 50);
  float smoke = random(30, 120);

  String payload = String("{\"lpg\":") + lpg + 
                   ",\"co\":" + co + 
                   ",\"smoke\":" + smoke + "}";

  String topic = "devices/" + String(deviceId) + "/messages/events/";

  Serial.print("Publishing: ");
  Serial.println(payload);

  mqttClient.publish(topic.c_str(), payload.c_str());
}


void setup() {
  Serial.begin(115200);
  connectToWiFi();
  
  // NTP Time Sync
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {  // Wait until time is synced
    delay(500);
    now = time(nullptr);
  }
  Serial.println("Time synchronized!");

  connectToAzureMQTT();
}

void loop() {
  if (!mqttClient.connected()) {
    connectToAzureMQTT();
  }
  mqttClient.loop();

  publishSensorData();
  delay(5000);  // every 5 seconds
}
