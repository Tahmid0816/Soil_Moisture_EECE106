#include <AD9833.h>      
#include <SPI.h>
#include <Preferences.h>

#define FNC_PIN 5        
#define ADC_PIN 34       
#define SAMPLES 1000     

AD9833 AD(FNC_PIN);
Preferences prefs;

float airValue, waterValue, freshSaltRatio;

void setup() {
  Serial.begin(115200);
  prefs.begin("soil_cal", false); 
  airValue = prefs.getFloat("air", 150.0);
  waterValue = prefs.getFloat("water", 1700.0);
  freshSaltRatio = prefs.getFloat("salt_base", 1.0); // New: Stores the "Fresh Water" ratio

  SPI.begin();
  AD.begin();
  AD.setWave(AD9833_SINE);
  
  Serial.println("\n--- Pro Sensor: Dual-Frequency Mode ---");
}

void loop() {
  // 1. Measure High Freq (Moisture)
  AD.setFrequency(95000);
  delay(50);
  float p2p_100k = getPeakToPeak();

  // 2. Measure Low Freq (Salinity)
  AD.setFrequency(5000);
  delay(50);
  float p2p_5k = getPeakToPeak();

  // 3. Handle Calibration Commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'D' || cmd == 'd') {
        airValue = p2p_100k;
        prefs.putFloat("air", airValue);
        Serial.println("Saved DRY Calibration.");
    }
    if (cmd == 'W' || cmd == 'w') {
        waterValue = p2p_100k;
        freshSaltRatio = p2p_5k / p2p_100k; // Save the "Fresh Water" ratio
        prefs.putFloat("water", waterValue);
        prefs.putFloat("salt_base", freshSaltRatio);
        Serial.println("Saved WET Calibration & Salt Baseline.");
    }
  }

  // 4. Calculate Corrected Values
  int moisturePercent = constrain((int)((p2p_100k - airValue) / (waterValue - airValue) * 100.0), 0, 100);
  
  // Normalized Salinity: Fresh water will now show ~1.0
  float currentRatio = p2p_5k / p2p_100k;
  float normalizedSalinity = currentRatio / freshSaltRatio;

  Serial.print("Moisture: "); Serial.print(moisturePercent); Serial.print("%");
  Serial.print(" | Raw Ratio: "); Serial.print(currentRatio);
  Serial.print(" | Salinity Score: "); Serial.println(normalizedSalinity);

  delay(1000);
}

float getPeakToPeak() {
  uint32_t maxVal = 0, minVal = 4095; 
  for (int i = 0; i < SAMPLES; i++) {
    uint32_t val = analogRead(ADC_PIN);
    if (val > maxVal) maxVal = val;
    if (val < minVal) minVal = val;
  }
  return (float)(maxVal - minVal);
}
