#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>

// ====== WiFi Config ======
const char* ssid = "Malevolent Shrine";
const char* password = "taizi2308";

// ====== Azure IoT Hub Device Connection String ======
const char* connectionString = "HostName=IEMDevice.azure-devices.net;mqttClientId=IEM_DEVICE_Air;SharedAccessKey=nUKNXf1OUrOAmMe7iUdVhiED52a62046lktsbjXb5p0=";

// ====== MQ2 Sensor Setup ======
#define MQ2_PIN 35  // GPIO34 (Input Only Pin)

//-------   MQ2 Sensor Calibration -------
#define GAS_LPG    (0)
#define GAS_CO     (1)
#define GAS_SMOKE  (2)

float LPGCurve[3]    = {2.3, 0.21, -0.47};  
float COCurve[3]     = {2.3, 0.72, -0.34};  
float SmokeCurve[3]  = {2.3, 0.53, -0.44};  
float Ro = 0.84; 

// ------- MQ2 Get Readings --------
float MQResistanceCalculation(int raw_adc) {
  return ((4095.0 / raw_adc) - 1.0) * 5.0; //load resistance = 5KOhm
}

float MQRead(int mq_pin) {
  int rs_adc = analogRead(mq_pin);
  return MQResistanceCalculation(rs_adc);
}

float MQGetGasPercentage(float rs_ro_ratio, int gas_id) {
  float x, y, slope;

  if (gas_id == GAS_LPG) {
    x = LPGCurve[0]; y = LPGCurve[1]; slope = LPGCurve[2];
  } else if (gas_id == GAS_CO) {
    x = COCurve[0]; y = COCurve[1]; slope = COCurve[2];
  } else if (gas_id == GAS_SMOKE) {
    x = SmokeCurve[0]; y = SmokeCurve[1]; slope = SmokeCurve[2];
  } else {
    return 0;
  }

  float log_ppm = (log10(rs_ro_ratio) - y) / slope + x;
  return pow(10, log_ppm);
}




// ====== DHT22 Sensor Setup ======
#define DHTPIN 4       // GPIO pin connected to DHT22
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====== Relay Setup ======
#define RELAY_PIN 25 

// ====== Timing and Averaging Setup ======
const unsigned long publishInterval = 10000; // 10 seconds
const unsigned long relayInterval = 5000;    // 5 seconds

unsigned long lastPublishTime = 0;
unsigned long lastRelaySampleTime = 0;

const int MAX_SAMPLES = 5;
unsigned long sampleInterval = 10000;  // 10 seconds
float temperatureSamples[MAX_SAMPLES];
float humiditySamples[MAX_SAMPLES];
int sampleIndex = 0;
unsigned long lastSampleTime = 0;

// ====== Average Calculation ======
float calculateAverage(float* samples, int count) {
  float sum = 0;
  for (int i = 0; i < count; i++) {
    sum += samples[i];
  }
  return (count > 0) ? (sum / count) : 0;
}

// ====== MQTT Settings ======
const char* mqttServer = "IEMDevice.azure-devices.net";
const int mqttPort = 8883;
const char* mqttClientId = "IEM_DEVICE_Air";
const char* mqttUsername = "IEMDevice.azure-devices.net/IEM_DEVICE_Air/?api-version=2020-09-30";

// ====== Generate SAS Token Separately ======
const char* mqttPassword = "SharedAccessSignature sr=IEMDevice.azure-devices.net%2Fdevices%2FIEM_DEVICE_Air&sig=ZdGiQlgZ1chdKyY%2Fm6gu%2BnL09vsNNRvuI2FJ%2FPvWetk%3D&se=1746259792";

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

   unsigned long currentMillis = millis();

  // ====== Sampling for Relay Logic every 5 seconds ======
  if (currentMillis - lastRelaySampleTime >= relayInterval) {
    lastRelaySampleTime = currentMillis;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    temperatureSamples[sampleIndex] = temperature;
    humiditySamples[sampleIndex] = humidity;
    sampleIndex++;

    if (sampleIndex >= MAX_SAMPLES) {
      float avgTemp = calculateAverage(temperatureSamples, MAX_SAMPLES);
      float avgHum = calculateAverage(humiditySamples, MAX_SAMPLES);

      Serial.print("Average Humidity for Relay: ");
      Serial.println(avgHum);

      if (avgHum >= 10.0) {
        digitalWrite(RELAY_PIN, LOW);  // Relay ON
        Serial.println("Relay ON - Diffuser ON (humidity >= 50%).");
      } else {
        digitalWrite(RELAY_PIN, HIGH); // Relay OFF
        Serial.println("Relay OFF - Diffuser OFF (humidity normal).");
      }

      sampleIndex = 0;  // Reset buffer
    }
  }

  // ====== Publishing to Azure every 10 seconds ======
  if (currentMillis - lastPublishTime >= publishInterval) {
    lastPublishTime = currentMillis;

    int raw_adc = analogRead(MQ2_PIN);
    Serial.print("Raw MQ2 Value: ");
    Serial.println(raw_adc);

    float rs = MQResistanceCalculation(raw_adc);
    float ratio = rs / Ro;

    float lpg = MQGetGasPercentage(ratio, GAS_LPG);
    float co = MQGetGasPercentage(ratio, GAS_CO);
    float smoke = MQGetGasPercentage(ratio, GAS_SMOKE);

      float temperature = dht.readTemperature();
      float humidity = dht.readHumidity();

      if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
      }

    String payload = String("{\"lpg\":") + lpg + 
                    ",\"co\":" + co + 
                    ",\"smoke\":" + smoke + 
                    ",\"temp\":" + temperature + 
                    ",\"humid\":" + humidity + "}";

    // MQTT Topic
    String topic = "devices/" + String(mqttClientId) + "/messages/events/";

    Serial.print("Publishing: ");
    Serial.println(payload);



  // Publish
  if (mqttClient.publish(topic.c_str(), payload.c_str())) {
    Serial.println("Message sent to Azure!");
  } else {
    Serial.println("Failed to send message.");
  }

  delay(10000); // Send data every 10 seconds
}
}