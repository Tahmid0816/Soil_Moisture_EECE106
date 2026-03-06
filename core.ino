#include <AD9833.h>      
#include <SPI.h>
#include <Preferences.h>

#define FNC_PIN 5        
#define ADC_PIN 34       
#define SAMPLES 1000     

AD9833 AD(FNC_PIN);
Preferences prefs;

float airValue95k;     // Air baseline at 95kHz
float airValue5k;      // Air baseline at 5kHz
float waterValue95k;   // Fresh water baseline at 95kHz
float waterValue5k;    // Fresh water baseline at 5kHz
float freshSaltRatio;  // Ratio of (5k/95k) in fresh water

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Initialize Preferences (NVS Storage)
  // "soil_cal" is the namespace, false means read/write mode
  prefs.begin("soil_cal", false); 

  // 2. Load Saved Calibration Data
  // Syntax: getFloat("key", defaultValue)
  airValue95k    = prefs.getFloat("air95", 150.0);   
  airValue5k     = prefs.getFloat("air5", 150.0);    
  waterValue95k  = prefs.getFloat("wat95", 1700.0);  
  waterValue5k   = prefs.getFloat("wat5", 1700.0);   
  freshSaltRatio = prefs.getFloat("salt_base", 1.0); 

  // 3. Initialize Hardware
  SPI.begin();
  AD.begin();
  AD.setWave(AD9833_SINE);

  Serial.println("\n--- Pro Sensor Initialized ---");
  Serial.println("Memory Loaded. Current Settings:");
  Serial.print("  Air 95k: "); Serial.println(airValue95k);
  Serial.print("  Air 5k:  "); Serial.println(airValue5k);
  Serial.print("  Wat 95k: "); Serial.println(waterValue95k);
  Serial.print("  Salt Base Ratio: "); Serial.println(freshSaltRatio);
  Serial.println("--------------------------------");
  Serial.println("Commands: 'D' = Calibrate Air | 'W' = Calibrate Fresh Water");
}

void loop() {
  // 1. Measure High Frequency (Moisture)
  AD.setFrequency(95000);
  delay(100); // Allow op-amp to stabilize
  float p2p_95k = getPeakToPeak();

  // 2. Measure Low Frequency (Salinity)
  AD.setFrequency(5000);
  delay(100);
  float p2p_5k = getPeakToPeak();

  // 3. Handle Calibration Commands
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    if (cmd == 'D' || cmd == 'd') {
      // Save AIR values for BOTH frequencies
      airValue95k = p2p_95k;
      airValue5k = p2p_5k;
      prefs.putFloat("air95", airValue95k);
      prefs.putFloat("air5", airValue5k);
      Serial.println(">>> CALIBRATED DRY (AIR) FOR BOTH FREQUENCIES");
    } 
    
    else if (cmd == 'W' || cmd == 'w') {
      // Save WET values for BOTH frequencies
      waterValue95k = p2p_95k;
      waterValue5k = p2p_5k;
      // Calculate the "Fresh Water" Ratio baseline
      // How much does 5k respond vs 95k in pure water?
      freshSaltRatio = (p2p_5k - airValue5k) / (p2p_95k - airValue95k + 1.0);
      
      prefs.putFloat("wat95", waterValue95k);
      prefs.putFloat("wat5", waterValue5k);
      prefs.putFloat("salt_base", freshSaltRatio);
      Serial.println(">>> CALIBRATED WET (FRESH WATER) & STORED SALT BASELINE");
    }
  }

  // 4. Calculate Moisture Percentage (Using 95kHz exclusively)
  // Formula: (Current - Air) / (Water - Air) * 100
  float moistureRange = waterValue95k - airValue95k;
  float moisturePercent = ((p2p_95k - airValue95k) / moistureRange) * 100.0;
  moisturePercent = constrain(moisturePercent, 0.0, 100.0);

  // 5. Calculate Salinity Score
  // We look at the "Gain" (how much the signal grew from Air)
  float gain5k = p2p_5k - airValue5k;
  float gain95k = p2p_95k - airValue95k;
  
  // Prevent division by zero and handle tiny air fluctuations
  if (gain95k < 10) gain95k = 10; 
  if (gain5k < 0) gain5k = 0;

  float currentRatio = gain5k / gain95k;
  
  // Normalize against Fresh Water: 1.0 = Fresh, >1.0 = Salty
  float salinityScore = currentRatio / freshSaltRatio;

  // 6. Output Results
  Serial.print("Moisture: ");
  Serial.print(moisturePercent, 1);
  Serial.print("% | ");

  Serial.print("Salinity Score: ");
  if (moisturePercent < 10.0) {
    Serial.println("0.00 (Too dry to measure)");
  } else {
    Serial.println(salinityScore, 2);
  }

  // Debug Data (Optional)
  /*
  Serial.print(" [Raw 95k: "); Serial.print(p2p_95k);
  Serial.print(" Raw 5k: "); Serial.print(p2p_5k);
  Serial.println("]");
  */

  delay(2000); // 2-second update rate
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
