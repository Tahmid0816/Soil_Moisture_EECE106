#define BLYNK_TEMPLATE_ID "TMPL6lU3Ha4mL"
#define BLYNK_TEMPLATE_NAME "Soil Moisture sensor"
#define BLYNK_AUTH_TOKEN "5fTKPk8J15jT68keywyi4KI98P7gvdyX"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <AD9833.h>      
#include <SPI.h>
#include <Preferences.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Your_WiFi_Name";
char pass[] = "Your_WiFi_Password";

#define FNC_PIN 5        
#define ADC_PIN 34       
#define SAMPLES 1000     

AD9833 AD(FNC_PIN);
Preferences prefs;
BlynkTimer timer;

float airValue95k, airValue5k, waterValue95k, waterValue5k, freshSaltRatio;
int currentMode = 1; // 0 = Sweep, 1 = Sensor

// --- Blynk Sync: Switch Mode from App ---
BLYNK_WRITE(V3) {
  currentMode = param.asInt(); 
  Serial.print("Mode switched via Blynk: ");
  Serial.println(currentMode == 0 ? "SWEEP" : "SENSOR");
}

void setup() {
  Serial.begin(115200);
  prefs.begin("soil_cal", false); 
  airValue95k = prefs.getFloat("air95", 150.0);   
  airValue5k  = prefs.getFloat("air5", 150.0);    
  waterValue95k = prefs.getFloat("wat95", 1700.0);  
  waterValue5k  = prefs.getFloat("wat5", 1700.0);   
  freshSaltRatio = prefs.getFloat("salt_base", 1.0); 

  SPI.begin();
  AD.begin();
  AD.setWave(AD9833_SINE);

  Blynk.begin(auth, ssid, pass);
  
  // Update Blynk every 2 seconds
  timer.setInterval(2000L, runSelectedMode);
}

void loop() {
  Blynk.run();
  timer.run();
  
  if (Serial.available() > 0) {
    handleCommand(Serial.read());
  }
}

void runSelectedMode() {
  if (currentMode == 0) {
    runSweepMode();
  } else {
    runSensorMode();
  }
}

// --- MODE 0: Sweep with Graph ---
void runSweepMode() {
  for (uint32_t freq = 5000; freq <= 95000; freq += 10000) {
    AD.setFrequency(freq);
    delay(40); // Small delay for hardware
    float p2p = getPeakToPeak();
    
    // Send to Blynk Graph (V0)
    Blynk.virtualWrite(V0, p2p); 
    
    Serial.print("Sweep Freq: "); Serial.println(freq);
    
    // Crucial: Keep Blynk alive during the for-loop
    Blynk.run(); 
  }
}

// --- MODE 1: Calibrated Sensor ---
void runSensorMode() {
  AD.setFrequency(95000); delay(80);
  float p2p_95k = getPeakToPeak();

  AD.setFrequency(5000); delay(80);
  float p2p_5k = getPeakToPeak();

  float moisturePercent = constrain(((p2p_95k - airValue95k) / (waterValue95k - airValue95k)) * 100.0, 0.0, 100.0);
  float gain5k = p2p_5k - airValue5k;
  float gain95k = p2p_95k - airValue95k;
  if (gain95k < 10) gain95k = 10;
  float salinityScore = (gain5k / gain95k) / freshSaltRatio;

  // Send to Blynk
  Blynk.virtualWrite(V1, moisturePercent);
  Blynk.virtualWrite(V2, salinityScore);

  Serial.print("M: "); Serial.print(moisturePercent);
  Serial.print("% | S: "); Serial.println(salinityScore);
}

// Helper: getPeakToPeak (Copy from your original code)
float getPeakToPeak() {
  uint32_t maxVal = 0, minVal = 4095; 
  for (int i = 0; i < SAMPLES; i++) {
    uint32_t val = analogRead(ADC_PIN);
    if (val > maxVal) maxVal = val;
    if (val < minVal) minVal = val;
  }
  return (float)(maxVal - minVal);
}

// Helper: handleCommand (Copy from your original code)
void handleCommand(char cmd) {
  if (cmd == 'M' || cmd == 'm') {
    currentMode = (currentMode == 0) ? 1 : 0;
    Blynk.virtualWrite(V3, currentMode); // Sync App button
  } 
  else if (cmd == 'D' || cmd == 'd') {
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
