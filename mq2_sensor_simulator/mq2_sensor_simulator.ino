#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
    Serial.begin(115200);
}

void loop() {
    float lpGas = random(200, 500) / 10.0;   // Simulated LP Gas (20.0 - 50.0 ppm)
    float smoke = random(100, 300) / 10.0;   // Simulated Smoke (10.0 - 30.0 ppm)
    float co = random(50, 150) / 10.0;     // Simulated CO (5.0 - 15.0 ppm)

    // Introduce Outliers (5% chance)
    if (random(1, 100) <= 5) {
        lpGas = random(800, 1000) / 10.0;   // LP Gas Outlier (80.0 - 100.0 ppm)
        smoke = random(500, 700) / 10.0;    // Smoke Outlier (50.0 - 70.0 ppm)
        co = random(200, 300) / 10.0;      // CO Outlier (20.0 - 30.0 ppm)
    }

     Serial.println(lpGas);
        Serial.println(smoke);
        Serial.println(co);
        Serial.println("--------------------------------------------------------------------------");

    delay(5000); // Send data every 5 seconds
}