#include <Tlc5940.h> // Ensure the TI TLC5940 library is installed

// TLC5940 Hardware Output Channels
const int CH_740NM = 0; 
const int CH_850NM = 1;

// Arduino Mega Analog Input Pins (From MCP602 Op-Amp)
const int pinRawDC = A0;   // Channel 1: Total Light (Used for mBLL Matrix Math)
const int pinPulseAC = A1; // Channel 2: High-Gain AC Pulse (Used for visual PPG/Heart Rate)

// Hardware-Software Balanced Brightness Limits
// 740nm runs full power to offset lower silicon sensitivity. 850nm is backed off ~20%.
const int brightness740 = 4095; // Full 11.8 mA current cap
const int brightness850 = 3276; // Capped at ~9.5 mA for sensor balance

// Multiplex Timing Window (15ms per phase gives a fast ~16Hz sweep rate)
const int flashDurationMs = 15; 

// Global mBLL Reference Baseline Variables
float I_baseline_740 = 0.0;
float I_baseline_850 = 0.0;
const float L_factor = 2.0; // Pathlength Factor (r * DPF = 0.5cm * 4.0)

void setup() {
  Serial.begin(115200); 
  Tlc.init();           
  Tlc.clear();          
  Tlc.update();         

  Serial.println("--- INTRANASAL fNIRS STARTUP ---");
  Serial.println("Please insert the probe. Settle countdown beginning...");

  // 5-Second Settle Delay Loop
  for (int i = 5; i > 0; i--) {
    Serial.print("Calibrating in: ");
    Serial.print(i);
    Serial.println(" seconds... KEEP PROBE STILL");
    delay(1000); 
  }

  // --- CAPTURE BASELINES (I_baseline) ---
  Serial.println("Freezing baseline optical reference frames...");
  
  // 1. Capture 740nm Baseline
  Tlc.set(CH_740NM, brightness740);
  Tlc.update();
  delay(10); // Let circuit settle
  float dark740 = getAmbientDC(); // Immediate dark sample
  I_baseline_740 = analogRead(pinRawDC) - dark740;

  // 2. Capture 850nm Baseline
  Tlc.clear();
  Tlc.set(CH_850NM, brightness850);
  Tlc.update();
  delay(10); // Let circuit settle
  float dark850 = getAmbientDC(); // Immediate dark sample
  I_baseline_850 = analogRead(pinRawDC) - dark850;

  // Shut off LEDs
  Tlc.clear();
  Tlc.update();

  // Safety check to ensure the probe isn't reading open air/0 light
  if (I_baseline_740 < 10.0 || I_baseline_850 < 10.0) {
    Serial.println("ERROR: Low baseline signal detected! Check probe placement and restart Arduino.");
    while(1); // Halt execution
  }

  Serial.println("Calibration complete! Streaming active data to Serial Plotter...");
  delay(1000);
}

void loop() {
  // --- PHASE 1: DARK FRAME ---
  Tlc.clear(); 
  Tlc.update(); 
  delay(2); 
  int ambientDC = analogRead(pinRawDC);
  int ambientAC = analogRead(pinPulseAC);
  delay(flashDurationMs);

  // --- PHASE 2: ACTIVE 740nm FLASH ---
  Tlc.set(CH_740NM, brightness740); 
  Tlc.update();
  delay(2); 
  float I_active_740 = analogRead(pinRawDC) - ambientDC; 
  int livePulseAC_740 = analogRead(pinPulseAC) - ambientAC; // Cleaned Ch2 pulse
  delay(flashDurationMs);

  // --- PHASE 3: DARK FRAME ---
  Tlc.clear();
  Tlc.update();
  delay(2); 
  ambientDC = analogRead(pinRawDC);
  ambientAC = analogRead(pinPulseAC);
  delay(flashDurationMs);

  // --- PHASE 4: ACTIVE 850nm FLASH ---
  Tlc.set(CH_850NM, brightness850); 
  Tlc.update();
  delay(2); 
  float I_active_850 = analogRead(pinRawDC) - ambientDC; 
  int livePulseAC_850 = analogRead(pinPulseAC) - ambientAC; // Cleaned Ch2 pulse
  delay(flashDurationMs);

  // --- PHASE 5: MATRIX MATH PROCESSING (mBLL) ---
  // Guard against illegal log(0) or negative light readings if probe moves
  if (I_active_740 > 1.0 && I_active_850 > 1.0) {
    
    // Compute Optical Density changes relative to our frozen setup baselines
    float deltaOD_740 = -log(I_active_740 / I_baseline_740);
    float deltaOD_850 = -log(I_active_850 / I_baseline_850);

    // Apply the inverted extinction coefficient math matrix
    float denominator = L_factor * ((1.15 * 1.06) - (0.78 * 0.40)); // Matrix constant (1.814)
    
    float delta_HbO2 = ((1.15 * deltaOD_850) - (0.78 * deltaOD_740)) / denominator;
    float delta_Hb   = ((1.06 * deltaOD_740) - (0.40 * deltaOD_850)) / denominator;

    // --- PHASE 6: SERIAL PLOTTER DATA OUTPUT ---
    // Format variables for immediate graphing inside the Arduino IDE Serial Plotter utility
    Serial.print("Delta_HbO2:");    Serial.print(delta_HbO2, 6);  Serial.print(",");
    Serial.print("Delta_Hb:");      Serial.print(delta_Hb, 6);    Serial.print(",");
    
    // Output Channel 2's high-gain AC pulse (scaled down dynamically to fit cleanly on the same graph axis)
    Serial.print("AC_Pulse_Wave:"); Serial.println(livePulseAC_850 * 0.001, 4);
  }
}

// Quick helper function used strictly during setup calibration
float getAmbientDC() {
  Tlc.clear();
  Tlc.update();
  delay(5);
  return analogRead(pinRawDC);
}
