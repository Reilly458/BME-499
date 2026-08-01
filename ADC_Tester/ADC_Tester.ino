#include "SparkFun_Tlc5940.h"

void updateLED1(int TLC_channel, int brightness){
  Tlc.clear();
  Tlc.set(TLC_channel, brightness);
  Tlc.update();
}

void LEDsoff(){
  Tlc.clear();
  Tlc.update();
  delay(10); // Give TLC physical time to latch the 0 values
}

void setup() {
  Serial.begin(115200);
  Tlc.init(); // Explicitly initialize the TLC chip
  
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  
  LEDsoff();
  Serial.println("--- System Ready ---");
}

void loop() {
  // ==========================================
  // STAGE 1: 740nm LED - ACTIVE FOR 4 SECONDS
  // ==========================================
  updateLED1(2, 4095);    
  delay(50); // Small delay for op-amp stabilization

  Serial.print("740nm LED ON -> ");
  
  // Take 4 readings spaced 1 second apart so you can track the DMM
  for(int i = 0; i < 4; i++){
    int raw = analogRead(0);
    float voltage = (5.0 * raw) / 1024.0;
    Serial.print("V: ");
    Serial.print(voltage, 2);
    Serial.print(" | ");
    delay(1000); // 1 second gap between visual checkpoints
  }
  Serial.println("[Done]");
  /*
  // Turn off and pause so you can see the "Dark" baseline
  LEDsoff();
  Serial.println("LEDs OFF (Dark Period)...");
  delay(2000); // 2 seconds of total darkness

  // ==========================================
  // STAGE 2: 850nm LED - ACTIVE FOR 4 SECONDS
  // ==========================================
  updateLED1(2, 4095); // Raised brightness to 4095 so you can see it match the 740nm test
  delay(50);

  Serial.print("850nm LED ON -> ");
  
  for(int i = 0; i < 4; i++){
    int raw = analogRead(1);
    float voltage = (5.0 * raw) / 1024.0;
    Serial.print("V: ");
    Serial.print(voltage, 2);
    Serial.print(" | ");
    delay(1000); 
  }
  Serial.println("[Done]");
  
  LEDsoff();
  Serial.println("LEDs OFF (Dark Period)...");
  Serial.println("--------------------------------------------");
  delay(2000); */
}
