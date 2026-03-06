#include <AD9833.h>      // Library: AD9833 by Rob Tillaart
#include <SPI.h>

// --- Pin Definitions ---
#define FNC_PIN 5        // Fsync / Chip Select for AD9833
#define ADC_PIN 34       // Connected to TL072 Output (Pin 1)
#define SAMPLES 500      // Number of ADC samples per reading

// --- AD9833 Object ---
AD9833 AD(FNC_PIN);      // Rob's library uses the 'AD' object naming usually

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize SPI and AD9833
  SPI.begin();           // Explicitly start SPI for ESP32
  AD.begin();            // Lowercase begin
  
  // Set initial signal: 10kHz Sine Wave
  AD.setWave(AD9833_SINE);
  AD.setFrequency(10000);
  
  Serial.println("--- Soil Impedance (Rob Tillaart Lib) Initialized ---");
  Serial.println("Frequency (Hz) | Peak-to-Peak (ADC) | Moisture (%)");
  Serial.println("--------------------------------------------------");
}

void loop() {
  // Step 1: Measure High Frequency (Moisture)
  AD.setFrequency(100000);
  delay(50);
  float moistureVal = getPeakToPeak();

  // Step 2: Measure Low Frequency (Salinity Sensitivity)
  AD.setFrequency(5000);
  delay(50);
  float salinitySensitivityVal = getPeakToPeak();

  // Step 3: Calculate the Ratio
  // If this number grows, your soil is getting saltier
  float saltIndex = salinitySensitivityVal / moistureVal;

  Serial.print("Moisture: "); Serial.print(moistureVal);
  Serial.print(" | Salinity Index: "); Serial.println(saltIndex);
  
  delay(2000);
}

// Function to find the "Height" of the AC Sine wave
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

// Calibration Mapper
int mapMoisture(float currentP2P) {
  // Update these after your first 'Dry' and 'Wet' test!
  float airValue = 150.0;   
  float waterValue = 1700.0; 
  
  int percent = map((int)currentP2P, (int)airValue, (int)waterValue, 0, 100);
  return constrain(percent, 0, 100);
}
