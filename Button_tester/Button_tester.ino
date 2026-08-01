#include <avr/interrupt.h>
#include "SparkFun_Tlc5940.h"

#define BUTN_OLFACTO 21
#define BUTN_THERAPY 20
#define OLFACTO 34      // Pseudo olfactometer mode
#define THERAPY 35      // Psesudo therapy mode
#define BUTN_STOP 18
#define BUTN_PAUSE 19
#define STOP 33
#define PAUSE 32

volatile int olfacto_state = 0;
volatile bool olfacto_pending = false;
volatile bool therapy_state = false;
volatile bool therapy_pending = false;
volatile bool stopped = false;
volatile bool running = true;
volatile unsigned long lastOMTime = 0;
volatile unsigned long lastTMTime = 0;
volatile unsigned long lastPMTime = 0;
const unsigned long debounceDelay = 250;

int loops = 0;
bool therapy_mode = false;
bool olfacto_mode = false;

ISR(INT0_vect){
  if(running && !olfacto_pending){
    olfacto_pending = true;
  } 
}

ISR(INT1_vect){
  if(running && !therapy_pending){
    therapy_pending = true;
  }
}

ISR(INT2_vect){
  unsigned long curTime = millis();
  if(curTime - lastPMTime > debounceDelay){
    running = !running;
    lastPMTime = curTime;
    EIFR = (1 << INTF2);
  }
}

ISR(INT3_vect){
  stopped = 1;
  EIFR = (1 << INTF3);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTN_OLFACTO, INPUT);
  pinMode(BUTN_THERAPY, INPUT);
  pinMode(OLFACTO, OUTPUT);
  pinMode(THERAPY, OUTPUT);
  pinMode(BUTN_STOP, INPUT);
  pinMode(BUTN_PAUSE, INPUT);
  pinMode(STOP, OUTPUT);
  pinMode(PAUSE, OUTPUT);

  EIFR = (1 << INTF3) | (1 << INTF2) | (1 << INTF1) | (1 << INTF0);
  EICRA |= (1 << ISC31) | (1 << ISC30) | (1 << ISC21) | (1 << ISC20) | (1 << ISC11) | (1 << ISC10) | (1 << ISC01) | (1 << ISC00);     // Set INT0 and INT1 to trigger on rising edge
  EIMSK |= (1 << INT3) | (1 << INT2) | (1 << INT1) | (1 << INT0);   // Enable INT0 and INT1 interupts 
  sei();
  Serial.println("Start Reading:");
}

void loop() {
  loops++;
  if(stopped){
    digitalWrite(STOP, HIGH);
    digitalWrite(OLFACTO, LOW);
    digitalWrite(THERAPY, LOW);
    digitalWrite(PAUSE, LOW);
  
    Serial.print("Program Stopped!, loop: ");
    Serial.println(loops);
    while(1);
  }

  if(!running){
    digitalWrite(PAUSE, HIGH);
    olfacto_state = 0;
    therapy_state = 0;
    Serial.print("System Paused. loop: ");
    Serial.println(loops);
    cli();
    olfacto_pending = false;
    therapy_pending = false;
    sei();
    delay(50);
    return;
  }

  digitalWrite(PAUSE, LOW);

  /*if(therapy_state || therapy_mode){    
    digitalWrite(THERAPY, HIGH);
    Serial.print("Therapy State: ");
    Serial.print(therapy_state);
    Serial.print(", Mode: ");
    Serial.print(therapy_mode);
    Serial.print(", loop: ");
    Serial.println(loops);
    therapy_mode = 1;
    therapy_state = 0;
    return;
  }*/
  if(therapy_pending){
    therapy_mode = !therapy_mode;
    Serial.print("Therapy pending. Loop: ");
    Serial.println(loops);
    cli();
    therapy_pending = false;
    sei();
  }

  if(olfacto_pending){
    Serial.print("Olfactometer pending. Loop: ");
    Serial.println(loops);
  
    olfacto_mode = !olfacto_mode;
    cli();
    olfacto_pending = false;
    sei();
  }

  if(therapy_mode){
    Serial.print("Therapy active. ");
    Serial.print(therapy_mode);
    Serial.print(" , Loop: ");
    Serial.println(loops);
    digitalWrite(THERAPY, HIGH);
    return;
  }
  else{
    digitalWrite(THERAPY, LOW);
  }

  if(olfacto_mode){
    Serial.print("Olfactometer active. ");
    Serial.print(olfacto_mode);
    Serial.print(" , Loop: ");
    Serial.println(loops);
    digitalWrite(OLFACTO, HIGH);
  }
  else{
    digitalWrite(OLFACTO, LOW);
  }
  /*if(olfacto_state){
    digitalWrite(OLFACTO, HIGH);
    Serial.print("Olfactometer Mode: ");
    Serial.println(olfacto_state);
    Serial.println(", loop: ");
    // = 0;
    //return;
    Serial.println(loops);
  }
  else{
    digitalWrite(OLFACTO, LOW);
  }*/
  //Serial.println(loops);
  delay(1000);
  
}
