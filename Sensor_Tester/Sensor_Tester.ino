#include "SparkFun_Tlc5940.h"

void setup() {
  Serial.begin(115200);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  analogReference(DEFAULT);
  Tlc.init();
  Serial.println("adc raw, adc volts");
}

void loop() {
  int adc0 = 0;
  float adcv0 = 0.0;
  Tlc.clear();
  Tlc.update();
  for(int i = 0; i< 20; i++){
    adc0 = analogRead(A0);
    adcv0 = (adc0*5)/1024; 
    Serial.print(adc0);
    Serial.print(", ");
    Serial.println(adcv0);
    delay(100);
  }
  
  Tlc.set(2, 4095);
  Tlc.update();
  for(int i = 0; i< 50; i++){
    adc0 = analogRead(A0);
    adcv0 = (adc0*5)/1024; 
    Serial.print(adc0);
    Serial.print(", ");
    Serial.println(adcv0);
    delay(100);
  }
  
  /*Tlc.set(1,0);
  Tlc.set(2, 4095);
  Tlc.update();
  for(int i = 0; i< 50; i++){
    adc0 = analogRead(A0);
    adcv0 = (adc0*5)/1024; 
    Serial.print(adc0);
    Serial.print(", ");
    Serial.println(adcv0);
    delay(100);
  }*/

}
