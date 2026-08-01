#include "SparkFun_Tlc5940.h"

//TLC5940 LED Driver Setup
#define DEOXY_740NM  1  //blue
#define OXY_850NM  2     //red
#define BRIGHT740  4095
#define BRIGHT850  4095 // decrease to 3276 to balance brightness

void updateLED1(int TLC_channel, int brightness){
  Tlc.clear();
  Tlc.set(TLC_channel, brightness);
  Tlc.update();
}

void LEDsoff(){
  Tlc.clear();
  Tlc.update();
}

void setup() {
  Serial.begin(115200);
  Tlc.init();
  LEDsoff();

}

void loop() {
  updateLED1(2, 4095);
 // Tlc.clear();
  //Tlc.set(1, 4095);
  //Tlc.set(2, 4095);
  //Tlc.update();
  delay(10);

}
