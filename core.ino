#include <AD9833.h>      
#include <SPI.h>
#include <Preferences.h>
#include <U8g2lib.h> 
#include <Wire.h>    
#include <WiFi.h>       
#include <HTTPClient.h> 

// --- NEW BUTTON PINS ---
#define BTN_MODE 13
#define BTN_DRY  12
#define BTN_WET  14
#define BTN_SYNC 27

// --- WiFi & Google Sheets Setup ---
const char* ssid = "Shah jalal";
const char* password = "shah1960";
const char* scriptID = "AKfycbx7d6XAiikOh70nGJUEFQ7O1XiZEysTvdu6Ek7emj5GiykqKrY_nveT3Hla4xSyDvyIHA"; 

#define FNC_PIN 5        
#define ADC_PIN 34       
#define SAMPLES 1000     

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

AD9833 AD(FNC_PIN);
Preferences prefs;

float airValue95k, airValue5k, waterValue95k, waterValue5k, freshSaltRatio;
int currentMode = 1; 

void setup() {
  Serial.begin(115200);
  delay(1000);

  // --- NEW BUTTON SETUP ---
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_DRY,  INPUT_PULLUP);
  pinMode(BTN_WET,  INPUT_PULLUP);
  pinMode(BTN_SYNC, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  u8g2.begin(); 
  
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
  showStatus();
}

void loop() {
  // --- NEW BUTTON LOGIC ---
  if (digitalRead(BTN_MODE) == LOW) { handleCommand('M'); delay(400); }
  if (digitalRead(BTN_DRY)  == LOW) { handleCommand('D'); delay(400); }
  if (digitalRead(BTN_WET)  == LOW) { handleCommand('W'); delay(400); }
  if (digitalRead(BTN_SYNC) == LOW) { handleCommand('S'); delay(400); }

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

// --- ALL OTHER FUNCTIONS (runSweepMode, runSensorMode, getPeakToPeak, handleCommand, showStatus) REMAIN 100% UNCHANGED ---

void runSweepMode() {
  Serial.println("\n--- Starting Frequency Sweep (5kHz - 95kHz) ---");
  for (uint32_t freq = 5000; freq <= 95000; freq += 10000) {
    AD.setFrequency(freq);
    delay(50);
    float p2p = getPeakToPeak();
    Serial.print("Freq: "); Serial.print(freq);
    Serial.print(" Hz \t| P2P: "); Serial.println(p2p);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 12, "MODE: SWEEP");
    u8g2.drawStr(0, 30, "Freq (Hz):");
    u8g2.setCursor(65, 30); u8g2.print(freq);
    u8g2.drawStr(0, 50, "P2P Value:");
    u8g2.setCursor(65, 50); u8g2.print(p2p, 1);
    u8g2.sendBuffer();
  }
  delay(2000); 
}

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
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "MODE: SENSOR");
  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.drawStr(0, 32, "Moist:");
  u8g2.setCursor(65, 32); u8g2.print(moisturePercent, 1); u8g2.print("%");
  u8g2.drawStr(0, 58, "Salt:");
  u8g2.setCursor(65, 58);
  if (moisturePercent < 5.0) u8g2.print("DRY");
  else u8g2.print(salinityScore, 2);
  u8g2.sendBuffer();
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

void handleCommand(char cmd) {
  if (cmd == 'M' || cmd == 'm') {
    currentMode = (currentMode == 0) ? 1 : 0;
    Serial.print("\n>>> Switched to Mode: ");
    Serial.println(currentMode == 0 ? "SWEEP" : "SENSOR");
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
  else if (cmd == 'S' || cmd == 's') {
    Serial.println(">>> SYNCING SWEEP TO GOOGLE SHEETS...");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 32, "SYNCING TO SHEETS...");
    u8g2.sendBuffer();
    for (uint32_t freq = 5000; freq <= 95000; freq += 10000) {
      AD.setFrequency(freq);
      delay(500);
      float p2p = getPeakToPeak();
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = "https://script.google.com/macros/s/" + (String)scriptID + "/exec?freq=" + String(freq) + "&p2p=" + String(p2p);
        http.begin(url.c_str());
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        int httpCode = http.GET();
        http.end();
        Serial.print("Sent Freq: "); Serial.println(freq);
      }
    }
    Serial.println(">>> SYNC COMPLETE.");
  }
}

void showStatus() {
  Serial.print("Mode: "); Serial.println(currentMode == 0 ? "SWEEP" : "SENSOR");
}
