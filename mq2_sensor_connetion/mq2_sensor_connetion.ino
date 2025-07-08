const int mq2Pin = 34;  // Analog pin connected to AO
const float RL = 5.0;   // Load resistance in kilo ohms
const float Ro = 10.0;  // Ro value (clean air resistance). Needs calibration!

void setup() {
  Serial.begin(115200);
}

void loop() {
  int adcValue = analogRead(mq2Pin);
  float voltage = adcValue * (3.3 / 4095.0);  // 12-bit ADC
  float Rs = ((3.3 - voltage) / voltage) * RL;  // Calculate Rs
  float ratio = Rs / Ro;

  // Estimate ppm using log-log equation from datasheet graph
  float ppmLPG = pow(10, ((-0.45) * log10(ratio)) + 2.95);
  float ppmCO = pow(10, ((-0.35) * log10(ratio)) + 3.1);
  float ppmSmoke = pow(10, ((-0.42) * log10(ratio)) + 3.54);

  Serial.print("ADC: "); Serial.print(adcValue);
  Serial.print(" | Voltage: "); Serial.print(voltage, 2);
  Serial.print(" V | Rs/Ro: "); Serial.print(ratio, 2);
  Serial.print(" | LPG: "); Serial.print(ppmLPG, 1);
  Serial.print(" ppm | CO: "); Serial.print(ppmCO, 1);
  Serial.print(" ppm | Smoke: "); Serial.print(ppmSmoke, 1);
  Serial.println(" ppm");

  delay(2000);
}
