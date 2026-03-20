#include <AD9833.h>      
#include <SPI.h>
#include <Preferences.h>

#define FNC_PIN 5        
#define ADC_PIN 34       
#define SAMPLES 1000     

AD9833 AD(FNC_PIN);
Preferences prefs;

// Calibration Variables
float airValue95k, airValue5k, waterValue95k, waterValue5k, freshSaltRatio;
int currentMode = 1; // 0 = Sweep, 1 = Calibrated Sensor

void setup() {
  Serial.begin(115200);
  delay(1000);

  prefs.begin("soil_cal", false); 
  airValue95k    = prefs.getFloat("air95", 150.0);   
  airValue5k     = prefs.getFloat("air5", 150.0);    
  waterValue95k  = prefs.getFloat("wat95", 1700.0);  
  waterValue5k   = prefs.getFloat("wat5", 1700.0);   
  freshSaltRatio = prefs.getFloat("salt_base", 1.0); 

  SPI.begin();
  AD.begin();
  AD.setWave(AD9833_SINE);

  Serial.println("\n--- Dual Mode Soil Analyzer ---");
  Serial.println("Commands: 'M' = Switch Mode | 'D' = Calibrate Air | 'W' = Calibrate Water");
  showStatus();
}

void loop() {
  // Check for incoming Serial commands
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }

  if (currentMode == 0) {
    runSweepMode();
  } else {
    runSensorMode();
  }
}

// --- MODE 0: Frequency Sweep ---
void runSweepMode() {
  Serial.println("\n--- Starting Frequency Sweep (5kHz - 95kHz) ---");
  for (uint32_t freq = 5000; freq <= 95000; freq += 10000) {
    AD.setFrequency(freq);
    delay(50);
    float p2p = getPeakToPeak();
    Serial.print("Freq: "); Serial.print(freq);
    Serial.print(" Hz \t| P2P: "); Serial.println(p2p);
  }
  delay(2000); 
}

// --- MODE 1: Calibrated Sensor ---
void runSensorMode() {
  // Measure High Freq
  AD.setFrequency(95000);
  delay(80);
  float p2p_95k = getPeakToPeak();

  // Measure Low Freq
  AD.setFrequency(5000);
  delay(80);
  float p2p_5k = getPeakToPeak();

  // Calculations
  float moisturePercent = constrain(((p2p_95k - airValue95k) / (waterValue95k - airValue95k)) * 100.0, 0.0, 100.0);
  float gain5k = p2p_5k - airValue5k;
  float gain95k = p2p_95k - airValue95k;
  if (gain95k < 10) gain95k = 10;
  float salinityScore = (gain5k / gain95k) / freshSaltRatio;

  Serial.print("MOISTURE: "); Serial.print(moisturePercent, 1); Serial.print("% | ");
  Serial.print("SALINITY SCORE: "); 
  if (moisturePercent < 5.0) Serial.println("N/A (Dry)");
  else Serial.println(salinityScore, 2);

  delay(1000);
}

// --- Helper Functions ---
float getPeakToPeak() {
  uint32_t maxVal = 0, minVal = 4095; 
  for (int i = 0; i < SAMPLES; i++) {
    uint32_t val = analogRead(ADC_PIN);
    if (val > maxVal) maxVal = val;
    if (val < minVal) minVal = val;
  }
  return (float)(maxVal - minVal);
}

void handleCommand(char cmd) {
  if (cmd == 'M' || cmd == 'm') {
    currentMode = (currentMode == 0) ? 1 : 0;
    Serial.print("\n>>> Switched to Mode: ");
    Serial.println(currentMode == 0 ? "SWEEP" : "SENSOR");
  } 
  else if (cmd == 'D' || cmd == 'd') {
    // We sample at 95k and 5k for the air baseline
    AD.setFrequency(95000); delay(100); airValue95k = getPeakToPeak();
    AD.setFrequency(5000);  delay(100); airValue5k = getPeakToPeak();
    prefs.putFloat("air95", airValue95k);
    prefs.putFloat("air5", airValue5k);
    Serial.println(">>> AIR CALIBRATED");
  }
  else if (cmd == 'W' || cmd == 'w') {
    AD.setFrequency(95000); delay(100); waterValue95k = getPeakToPeak();
    AD.setFrequency(5000);  delay(100); waterValue5k = getPeakToPeak();
    freshSaltRatio = (waterValue5k - airValue5k) / (waterValue95k - airValue95k + 1.0);
    prefs.putFloat("wat95", waterValue95k);
    prefs.putFloat("wat5", waterValue5k);
    prefs.putFloat("salt_base", freshSaltRatio);
    Serial.println(">>> WATER CALIBRATED");
  }
}

void showStatus() {
  Serial.print("Mode: "); Serial.println(currentMode == 0 ? "SWEEP" : "SENSOR");
}
