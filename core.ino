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
  // Sweep from 5kHz to 100kHz
  for (uint32_t freq = 5000; freq <= 100000; freq += 10000) {
    AD.setFrequency(freq);
    delay(50); // Stabilization delay
    
    float p2p = getPeakToPeak();
    int moisturePercent = mapMoisture(p2p);

    Serial.print(freq);
    Serial.print("\t\t| ");
    Serial.print(p2p);
    Serial.print("\t\t| ");
    Serial.println(moisturePercent);
  }

  Serial.println("--- Sweep Complete. Resting... ---");
  delay(5000); 
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
