#include <AD9833.h>      
#include <SPI.h>
#include <Preferences.h> // Library to save calibration to Flash

// --- Pin Definitions ---
#define FNC_PIN 5        
#define ADC_PIN 34       
#define SAMPLES 1000     // Increased samples for better stability

// --- Global Objects ---
AD9833 AD(FNC_PIN);
Preferences prefs;

// --- Calibration Variables ---
float airValue;
float waterValue;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // 1. Initialize Flash Memory
  prefs.begin("soil_cal", false); 
  airValue = prefs.getFloat("air", 150.0);    // Default to 150 if empty
  waterValue = prefs.getFloat("water", 1700.0); // Default to 1700 if empty

  // 2. Initialize Hardware
  SPI.begin();
  AD.begin();
  AD.setWave(AD9833_SINE);
  
  Serial.println("\n--- Pro Soil & Salinity Sensor ---");
  Serial.println("Commands: Send 'D' for DRY (Air) | Send 'W' for WET (Water)");
  Serial.print("Current Cal: Air="); Serial.print(airValue);
  Serial.print(" | Water="); Serial.println(waterValue);
}

void loop() {
  // Check for Calibration Commands
  handleCalibration();

  // Sweep and Calculate
  Serial.println("\nFreq(Hz)\t| P2P\t| Moisture%\t| Salt-Idx");
  Serial.println("---------------------------------------------------------");

  float p2p_5k = 0;
  float p2p_100k = 0;

  for (uint32_t freq = 5000; freq <= 100000; freq += 45000) { // Test 5k, 50k, 95k
    AD.setFrequency(freq);
    delay(50);
    
    float p2p = getPeakToPeak();
    
    // Store specific values for Salinity Index
    if (freq == 5000) p2p_5k = p2p;
    if (freq >= 95000) p2p_100k = p2p;

    int moisturePercent = calculateMoisture(p2p);
    
    Serial.print(freq);
    Serial.print("\t| ");
    Serial.print(p2p, 0);
    Serial.print("\t| ");
    Serial.print(moisturePercent);
    Serial.println("%");
  }

  // Calculate Salinity Index (Ratio of Low Freq to High Freq)
  float saltIndex = p2p_5k / p2p_100k;
  Serial.print("--- SALINITY INDEX: "); Serial.println(saltIndex, 2);

  delay(3000); 
}

// --- Helper Functions ---

float getPeakToPeak() {
  uint32_t maxVal = 0;
  uint32_t minVal = 4095; 
  
  for (int i = 0; i < SAMPLES; i++) {
    uint32_t val = analogRead(ADC_PIN);
    if (val > maxVal) maxVal = val;
    if (val < minVal) minVal = val;
  }
  return (float)(maxVal - minVal);
}

void handleCalibration() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    float currentP2P = getPeakToPeak();

    if (cmd == 'D' || cmd == 'd') {
      airValue = currentP2P;
      prefs.putFloat("air", airValue);
      Serial.print("\n>>> CALIBRATED DRY: "); Serial.println(airValue);
    } 
    else if (cmd == 'W' || cmd == 'w') {
      waterValue = currentP2P;
      prefs.putFloat("water", waterValue);
      Serial.print("\n>>> CALIBRATED WET: "); Serial.println(waterValue);
    }
  }
}

int calculateMoisture(float currentP2P) {
  // Linear interpolation: Percentage = (val - air) / (water - air) * 100
  float percent = ((currentP2P - airValue) / (waterValue - airValue)) * 100.0;
  return constrain((int)percent, 0, 100);
}
