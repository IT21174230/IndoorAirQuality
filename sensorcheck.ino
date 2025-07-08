#include <DHT.h>

// ====== Define Sensor Type & Pin ======
#define DHTPIN 4        // GPIO pin connected to the data pin of DHT22
#define DHTTYPE DHT22   // DHT 22 (AM2302)

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  Serial.println("DHT22 test starting...");
  dht.begin();
}

void loop() {
  delay(2000); // Wait 2 seconds between readings

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT22 sensor!");
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C  |  Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
}
