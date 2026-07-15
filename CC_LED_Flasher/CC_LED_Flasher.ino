#include "SparkFun_Tlc5940.h"

//TLC5940 LED Driver Setup
const int DEOXY_740NM = 1;  //blue
const int OXY_850NM = 2;     //red
const int BRIGHT740 = 4095;
const int BRIGHT850 = 4095; // decrease to 3276 to balance brightness

//Op Amp Config
const int pinRawDC = A0;    // use this for calculations
const int pinPulseAC = A1;  // use this for output visual

//Timing
const int flash_ms = 1500;    //15ms ~ 16Hz sweep rate

//mBLL variables
float I_base740 = 0.0;
float I_base850 = 0.0;
int ambientDC = 0;
int ambientAC = 0;
float DPF = 3.5;    //~3.5-4 fpr mucous membrane tissue
const float r = 0.0;    //distance between center of led and center of photodiode, mm or um most likely
const float L_DPF = r * DPF; 

volatile int phase = 0;


void updateLED1(int TLC_channel, int brightness, int delay_ms){
  Tlc.clear();
  Tlc.set(TLC_channel, brightness);
  Tlc.update();
  delay(delay_ms);
}

void LEDsoff(){
  Tlc.clear();
  Tlc.update();
}

void getBaseline(int TLC_channel, int brightness){
  LEDsoff();
  delay(2);
  float dark = analogRead(pinRawDC);    //Measure dark ambient light with both LEDs off, raw ADC value
  Serial.print("Dark current = ");
  Serial.println(dark);
  updateLED1(TLC_channel, brightness, flash_ms);    //Turn on 1 LED, 15ms delay for opamp settling
  if(TLC_channel == 1){ 
    Serial.print("740nm Baseline readings: ");
    for(int i =0; i < 5; i++){
      if (i == 0){
        I_base740 = (I_base740 + analogRead(pinRawDC) - dark) / 1;  
      }
      else{ I_base740 = (I_base740 + analogRead(pinRawDC) - dark) / 2; }   //Read LED on signal and subtract off dark ADC value
      Serial.print(I_base740);
      Serial.print(", ");
      delay(200);
    }
    Serial.println();
  }
  else{
    Serial.print("850nm Baseline readings: ");
    for(int i =0; i < 5; i++){
      if (i == 0){
        I_base850 = (I_base850 + analogRead(pinRawDC) - dark) / 1;    //Read LED on signal and subtract off dark ADC value
      }
      else{ I_base850 = (I_base850 + analogRead(pinRawDC) - dark) / 2; }   //Read LED on signal and subtract off dark ADC value
      Serial.print(I_base740);
      Serial.print(", ");
      delay(200);
    }
    Serial.println();  
  }
  LEDsoff();
}

void calibrate(){
  Serial.println("Calibrating. Hold Probe still.");
  for(int i = 5; i >= 0; i--){
    Serial.println(i);
    delay(1000);   
  }
  Serial.println("Getting baseline dark current values for each LED.");
  getBaseline(DEOXY_740NM, BRIGHT740);
  getBaseline(OXY_850NM, BRIGHT850);
  // Safety check to ensure the probe isn't reading open air/0 light
  if (I_base740 < 1.0 || I_base850 < 1.0) {
    Serial.println("ERROR: Low baseline signal detected! Check probe placement and restart Arduino.");
    while(1); // Halt execution
  }
  Serial.println("Calibration Complete.");
  delay(1000);
}

void printSample(int ambientDC, int ambientAC, float active740, int lpulse740, float active850, int lpulse850, float mBLL){
  Serial.print(millis());
  Serial.print(",");
  Serial.print(ambientDC);
  Serial.print(",");
  Serial.print(ambientAC);
  Serial.print(",");
  Serial.print(active740);
  Serial.print(",");
  Serial.print(lpulse740);
  Serial.print(",");
  Serial.print(active850);
  Serial.print(",");
  Serial.print(lpulse850);
  Serial.print(",");
  Serial.println(mBLL);
  
}

void setup() {
  Serial.begin(9600);
  pinMode(pinRawDC, INPUT);
  pinMode(pinPulseAC, INPUT);
  analogReference(DEFAULT);
  Tlc.init();
  LEDsoff();
  calibrate();
  Serial.println("Time, I Active 740, Live Pulse 740, I Active 850, Live Pulse 850, mBLL [mMoles], Ambient DC, Ambient AC, I Base 740, I Base 850");
  Serial.print(millis());
  Serial.print(", ");
  Serial.print("0, 0, 0, 0, 0 ");
  Serial.print(ambientDC);
  Serial.print(", ");
  Serial.print(ambientAC);
  Serial.print(", ");
  Serial.print(I_base740);
  Serial.print(", ");
  Serial.println(I_base850);
}

void loop() {
  //Serial.println("stuck at start of main loop line 125");

  switch(phase){    
    case 0:   //Dark phase, all LEDs off
      LEDsoff();
      delay(2);
      for(int i = 0; i < 5; i++){
        ambientDC = analogRead(pinRawDC);
        ambientAC = analogRead(pinPulseAC);
        printSample(ambientDC, ambientAC, 0, 0, 0, 0, 0);
        delay(flash_ms/5);
      }    
      break; 

    case 1:   //740nm LED active
      updateLED1(DEOXY_740NM, BRIGHT740,2);    
      for(int i = 0; i < 5; i++){
        float I_Active740 = analogRead(pinRawDC) - ambientDC; 
        int livePulseAC740 = analogRead(pinPulseAC) - ambientAC;
        printSample(ambientDC, ambientAC, I_Active740, livePulseAC740, 0, 0, 0);
        delay(flash_ms/5);
      }
      break;
  
    case 2:   //Dark phase, all LEDs off
      LEDsoff();  
      delay(2);
      for(int i = 0; i < 5; i++){    
        ambientDC = analogRead(pinRawDC);
        ambientAC = analogRead(pinPulseAC);
        printSample(ambientDC, ambientAC, 0, 0, 0, 0, 0);  
        delay(flash_ms/5);
      }
      break;

    case 3:   //850nm LED active
      updateLED1(OXY_850NM, BRIGHT850,2);
      for(int i = 0; i < 5; i++){      
        float I_Active850 = analogRead(pinRawDC) - ambientDC; 
        int livePulseAC850 = analogRead(pinPulseAC) - ambientAC;
        printSample(ambientDC, ambientAC, 0, 0, I_Active850, livePulseAC850, 0);
        delay(flash_ms/5);
      }
      break;
  }  
  phase = (phase + 1) % 4;
}
  /*
  Tlc.set(1,0);
  Tlc.set(2, 4095);
  Tlc.update();
  for(int i= 0; i < 10; i++){
    int raw_adc0 = analogRead(A0);
    float adc_vo0 = 5.0*(float(raw_adc0)/1023.0);
    int raw_adc1 = analogRead(A1);
    float adc_vo1 = 5.0*(float(raw_adc1)/1023.0);
    Serial.print(millis());
    Serial.print(",");
    Serial.print(raw_adc0);
    Serial.print(",");
    Serial.print(adc_vo0);
    Serial.print(",");
    Serial.print(raw_adc1);
    Serial.print(",");
    Serial.println(adc_vo1);
    delay(100);
  }
  //delay(1000);
  Tlc.clear();
  Tlc.update();
  for(int i= 0; i < 10; i++){
    int raw_adc0 = analogRead(A0);
    float adc_vo0 = 5.0*(float(raw_adc0)/1023.0);
    int raw_adc1 = analogRead(A1);
    float adc_vo1 = 5.0*(float(raw_adc1)/1023.0);
    Serial.print(millis());
    Serial.print(",");
    Serial.print(raw_adc0);
    Serial.print(",");
    Serial.print(adc_vo0);
    Serial.print(",");
    Serial.print(raw_adc1);
    Serial.print(",");
    Serial.println(adc_vo1);
    delay(100);
  }

  Tlc.set(1, 4095);
  Tlc.set(2, 0);
  Tlc.update();
  for(int i= 0; i < 10; i++){
    int raw_adc0 = analogRead(A0);
    float adc_vo0 = 5.0*(float(raw_adc0)/1023.0);
    int raw_adc1 = analogRead(A1);
    float adc_vo1 = 5.0*(float(raw_adc1)/1023.0);
    Serial.print(millis());
    Serial.print(",");
    Serial.print(raw_adc0);
    Serial.print(",");
    Serial.print(adc_vo0);
    Serial.print(",");
    Serial.print(raw_adc1);
    Serial.print(",");
    Serial.println(adc_vo1);
    delay(100);
  }
  //delay(1000);
  Tlc.clear();
  Tlc.update();
  for(int i= 0; i < 10; i++){
    int raw_adc0= analogRead(A0);
    float adc_vo0 = 5.0*(float(raw_adc0)/1023.0);
    int raw_adc1 = analogRead(A1);
    float adc_vo1 = 5.0*(float(raw_adc1)/1023.0);
    Serial.print(millis());
    Serial.print(",");
    Serial.print(raw_adc0);
    Serial.print(",");
    Serial.print(adc_vo0);
    Serial.print(",");
    Serial.print(raw_adc1);
    Serial.print(",");
    Serial.println(adc_vo1);
    delay(100);
  }
}*/


/*
#define LED_PIN 9         // Pin driving your fNIRS LED modulation circuit
#define ANALOG_PIN A0     // Pin connected to your photodiode TIA output

// Lock-In Configuration (Assuming 4000Hz sampling for a 500Hz LED wave)
const int ref_sine[8] = {0, 707, 1000, 707, 0, -707, -1000, -707}; // Scaled by 1000 to avoid floats
volatile int sample_index = 0;
volatile long lockin_output = 0;

// Filter Alpha for low pass (Higher = smoother but slower response. Must be between 0 and 1000)
const long ALPHA = 10; 

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Configure Timer 1 for a precise 4000 Hz (250 microseconds) Interrupt
  cli();                      // Disable global interrupts
  TCCR1A = 0;                 // Set entire TCCR1A register to 0
  TCCR1B = 0;                 // Set entire TCCR1B register to 0
  TCNT1  = 0;                 // Initialize counter value to 0
  
  // Set compare match register for 4000 Hz increment
  // Formula: (16,000,000 Hz / (Prescaler * Desired Frequency)) - 1
  // (16000000 / (1 * 4000)) - 1 = 3999
  OCR1A = 3999;             
  
  TCCR1B |= (1 << WGM12);     // Turn on CTC (Clear Timer on Compare Match) mode
  TCCR1B |= (1 << CS10);      // Set CS10 bit for No Prescaler
  TIMSK1 |= (1 << OCIE1A);    // Enable timer compare interrupt
  
  // Speed up the Mega 2560 ADC (Set Prescaler to 32 instead of default 128)
  // This drops ADC conversion time from ~100us down to ~26us without losing precision
  ADCSRA &= ~(1 << ADPS2);
  ADCSRA |= (1 << ADPS1);
  ADCSRA |= (1 << ADPS0);
  
  sei();                      // Enable global interrupts
}

// Precise Timer 1 Interrupt Service Routine (Runs at 4000 Hz)
ISR(TIMER1_COMPA_vect) {
  // 1. Read ADC quickly (Direct register reading bypassing analogRead overhead)
  int raw_adc = analogRead(ANALOG_PIN); 

  // 2. Lock-In Demodulation (Multiply signal by synchronized reference)
  long multiplied = (long)raw_adc * ref_sine[sample_index];

  // 3. Digital Low-Pass Filter (Exponential Moving Average using Fixed-Point Math)
  // Formula: filtered = (ALPHA * multiplied + (1000 - ALPHA) * last_filtered) / 1000
  lockin_output = (ALPHA * multiplied + (1000 - ALPHA) * lockin_output) / 1000;

  // 4. Advance sample index (loops 0 to 7)
  sample_index = (sample_index + 1) % 8;
  
  // Optional: Toggle your LED pin here if you want the Mega to handle the 500Hz square-wave generation
  // (e.g., Turn ON if sample_index < 4, Turn OFF if sample_index >= 4)
}

void loop() {
  // Print out the extracted DC hemodynamic signal every 50ms
  static unsigned long last_print = 0;
  if (millis() - last_print > 50) {
    last_print = millis();
    
    // Read the volatile variable safely by temporarily halting interrupts
    cli();
    long print_val = lockin_output;
    sei();

    // Out value represents the isolated absorption data at 500 Hz
    Serial.println(print_val); 
  }
}
*\

/*
#define ANALOG_PIN A0     // Photodiode TIA output

// TLC5940 direct pin control or bit-banging pins (Adjust to match your wiring)
// For simplicity, we assume you are driving the TLC5940 channels via active-low logic
#define TLC_740_PIN 8     // Controls 740nm channel (OUT0)
#define TLC_850_PIN 9     // Controls 850nm channel (OUT1)

// Lock-in reference values for a 4-step sequence (Square-wave demodulation)
// Phase 0: 740nm Active, Phase 1: Dark, Phase 2: 850nm Active, Phase 3: Dark
volatile int current_phase = 0;

// Extracted outputs for both wavelengths
volatile long lockin_740 = 0;
volatile long lockin_850 = 0;
volatile long dark_baseline = 0;

// Filter Alpha for low pass (0 to 1000. Lower = smoother/slower)
const long ALPHA = 15; 

void setup() {
  Serial.begin(115200);
  
  pinMode(TLC_740_PIN, OUTPUT);
  pinMode(TLC_850_PIN, OUTPUT);
  
  // TLC5940 channels are active LOW (LOW turns LED ON, HIGH turns LED OFF)
  digitalWrite(TLC_740_PIN, HIGH); 
  digitalWrite(TLC_850_PIN, HIGH);

  cli(); // Disable interrupts
  
  // Configure Timer 1 for 2000 Hz Interrupt (500us per phase)
  // Configure Timer 1 for 4000 Hz Interrupt (250us per phase)
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = 7999; // (16,000,000 / (1 * 2000)) - 1 = 7999
  TCCR1B |= (1 << WGM12); // CTC Mode
  TCCR1B |= (1 << CS10);  // No Prescaler
  TIMSK1 |= (1 << OCIE1A);
  
  // Speed up ADC (Prescaler 32)
  ADCSRA &= ~(1 << ADPS2);
  ADCSRA |= (1 << ADPS1);
  ADCSRA |= (1 << ADPS0);
  
  sei(); // Enable interrupts
}

// ISR triggers every 500 microseconds (2000 Hz)
ISR(TIMER1_COMPA_vect) {
  // 1. Read the photodiode voltage resulting from the PREVIOUS phase's lighting
  int raw_adc = analogRead(ANALOG_PIN);

  // 2. Process data based on the phase that just completed
  switch (current_phase) {
    case 0: // Just finished 740nm ON phase
      // Subtract the last known dark baseline to isolate the 740nm signal
      lockin_740 = (ALPHA * (raw_adc - dark_baseline) + (1000 - ALPHA) * lockin_740) / 1000;
      break;
      
    case 1: // Just finished Dark Phase 1
      dark_baseline = (ALPHA * raw_adc + (1000 - ALPHA) * dark_baseline) / 1000;
      break;
      
    case 2: // Just finished 850nm ON phase
      // Subtract the last known dark baseline to isolate the 850nm signal
      lockin_850 = (ALPHA * (raw_adc - dark_baseline) + (1000 - ALPHA) * lockin_850) / 1000;
      break;
      
    case 3: // Just finished Dark Phase 2
      dark_baseline = (ALPHA * raw_adc + (1000 - ALPHA) * dark_baseline) / 1000;
      break;
  }

  // 3. Switch to the NEXT phase state
  current_phase = (current_phase + 1) % 4;

  // 4. Update TLC5940 physical pins for the upcoming phase
  if (current_phase == 0) {
    digitalWrite(TLC_740_PIN, LOW);  // Turn ON 740nm
    digitalWrite(TLC_850_PIN, HIGH); // Turn OFF 850nm
  } 
  else if (current_phase == 2) {
    digitalWrite(TLC_740_PIN, HIGH); // Turn OFF 740nm
    digitalWrite(TLC_850_PIN, LOW);  // Turn ON 850nm
  } 
  else {
    // Phases 1 and 3 are Dark phases
    digitalWrite(TLC_740_PIN, HIGH); // Turn OFF 740nm
    digitalWrite(TLC_850_PIN, HIGH); // Turn OFF 850nm
  }
}

void loop() {
  static unsigned long last_print = 0;
  if (millis() - last_print > 100) { // Print data at 10 Hz
    last_print = millis();
    
    cli(); // Temporarily pause interrupts for clean reading
    long out_740 = lockin_740;
    long out_850 = lockin_850;
    sei();

    // Output formatted for the Arduino Serial Plotter
    Serial.print("740nm:");
    Serial.print(out_740);
    Serial.print(",");
    Serial.print("850nm:");
    Serial.println(out_850);
  }
}
*/

/* with ripple counter
#define ANALOG_PIN A0     // Photodiode TIA output

// Synchronization Pins from CD4040BE
#define Q0_PIN 2          // Connects to CD4040BE Bit 0 output
#define Q1_PIN 3          // Connects to CD4040BE Bit 1 output

// Extracted outputs for both wavelengths
volatile long lockin_740 = 0;
volatile long lockin_850 = 0;
volatile long dark_baseline = 0;

// Filter Alpha for low pass (0 to 1000. Lower = smoother/slower)
const long ALPHA = 15; 

void setup() {
  Serial.begin(115200);
  
  pinMode(Q0_PIN, INPUT);
  pinMode(Q1_PIN, INPUT);

  cli(); // Disable interrupts
  
  // Attach external interrupts to Pins 2 and 3
  // "CHANGE" triggers the interrupt on both rising and falling edges of the CD4040BE bits
  attachInterrupt(digitalPinToInterrupt(Q0_PIN), sync_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(Q1_PIN), sync_isr, CHANGE);
  
  // Speed up Arduino Mega ADC (Prescaler 32) so the read finishes in ~26 microseconds
  ADCSRA &= ~(1 << ADPS2);
  ADCSRA |= (1 << ADPS1);
  ADCSRA |= (1 << ADPS0);
  
  sei(); // Enable interrupts
}

// This Interrupt Service Routine triggers every time the CD4040BE advances state
void sync_isr() {
  // 1. Immediately sample the photodiode voltage 
  int raw_adc = analogRead(ANALOG_PIN);

  // 2. Read the hardware state of the counter to know what phase we just read
  // (Using fast direct port reading of PINE/PIND is ideal, but digitalRead works here)
  bool q0 = digitalRead(Q0_PIN);
  bool q1 = digitalRead(Q1_PIN);
  
  // Reconstruct the 2-bit phase index (0 to 3)
  int hardware_phase = (q1 << 1) | q0;

  // 3. Sort and process the ADC value based on the hardware state
  switch (hardware_phase) {
    case 0: // Hardware is currently in 740nm ON state
      // Subtract the last known dark baseline to isolate the 740nm signal
      lockin_740 = (ALPHA * (raw_adc - dark_baseline) + (1000 - ALPHA) * lockin_740) / 1000;
      break;
      
    case 1: // Hardware is currently in Dark Phase 1
      dark_baseline = (ALPHA * raw_adc + (1000 - ALPHA) * dark_baseline) / 1000;
      break;
      
    case 2: // Hardware is currently in 850nm ON state
      // Subtract the last known dark baseline to isolate the 850nm signal
      lockin_850 = (ALPHA * (raw_adc - dark_baseline) + (1000 - ALPHA) * lockin_850) / 1000;
      break;
      
    case 3: // Hardware is currently in Dark Phase 2
      dark_baseline = (ALPHA * raw_adc + (1000 - ALPHA) * dark_baseline) / 1000;
      break;
  }
}

void loop() {
  static unsigned long last_print = 0;
  if (millis() - last_print > 100) { // Print data at 10 Hz
    last_print = millis();
    
    cli(); // Temporarily pause interrupts for a clean atomic read
    long out_740 = lockin_740;
    long out_850 = lockin_850;
    sei();

    // Stream out formatted data for the Arduino Serial Plotter
    Serial.print("740nm:");
    Serial.print(out_740);
    Serial.print(",");
    Serial.print("850nm:");
    Serial.println(out_850);
  }
}
*/
