#include "SparkFun_Tlc5940.h"

//#define LED_PIN 9         // Pin driving your fNIRS LED modulation circuit
//#define ANALOG_PIN A0     // Pin connected to your photodiode TIA output

//TLC5940 LED Driver Setup
#define DEOXY_740NM  1  //blue
#define OXY_850NM  2     //red
#define BRIGHT740  4095
#define BRIGHT850  4095 // decrease to 3276 to balance brightness

//Op Amp Config
#define pinRawDC  A0    // use this for calculations
#define pinPulseAC  A1  // use this for output visual

//Timing
const int flash_ms = 1500;    //15ms ~ 16Hz sweep rate

//mBLL variables
float I_base740 = 0.0;
float I_base850 = 0.0;
int ambientDC = 0;
int ambientAC = 0;
int livePulseAC740 = 0;
int livePulseAC850 = 0;
float I_Active740 = 0.0;
float I_Active850 = 0.0;
float DPF = 3.5;    //~3.5-4 fpr mucous membrane tissue
const float r = 0.0;    //distance between center of led and center of photodiode, mm or um most likely
const float L_DPF = r * DPF; 

volatile int phase = 0;
// Lock-In Configuration (Assuming 4000Hz sampling for a 500Hz LED wave)
//const int ref_sine[8] = {0, 707, 1000, 707, 0, -707, -1000, -707}; // Scaled by 1000 to avoid floats
volatile int sample_index = 0;
//volatile long lockin_output = 0;

// Filter Alpha for low pass (Higher = smoother but slower response. Must be between 0 and 1000)
//const long ALPHA = 10; 

void updateLED1(int TLC_channel, int brightness){
  Tlc.clear();
  Tlc.set(TLC_channel, brightness);
  Tlc.update();
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
  updateLED1(TLC_channel, brightness);
  delay(flash_ms);    //Turn on 1 LED, 15ms delay for opamp settling
  if(TLC_channel == 1){ 
    Serial.print("740nm Baseline readings: ");
    for(int i =0; i < 5; i++){
      I_base740 = (i==0) ? (I_base740 + analogRead(pinRawDC) - dark) / 1 : (I_base740 + analogRead(pinRawDC) - dark) / 2;     //Read LED on signal and subtract off dark ADC value
      Serial.print(I_base740);
      Serial.print(", ");
      delay(200);
    }
    Serial.println();
  }
  else{
    Serial.print("850nm Baseline readings: ");
    for(int i =0; i < 5; i++){
      I_base850 = (i==0) ? (I_base850 + analogRead(pinRawDC) - dark) / 1 : (I_base850 + analogRead(pinRawDC) - dark) / 2;
      Serial.print(I_base740);
      Serial.print(", ");
      delay(200);
    }
    Serial.println();  
  }
  LEDsoff();
  delay(500);
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
    Serial.print("740 Baseline: ");
    Serial.println(I_base740);
    Serial.print("850 Baseline: ");
    Serial.println(I_base850);
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

// Optimized high-speed ADC read function (takes ~16 microseconds)
inline int fastAnalogRead(uint8_t ch) {
  #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = (ADMUX & 0xE0) | (ch & 0x07);
  #else
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
  #endif
  ADCSRA |= (1 << ADSC);           
  while (ADCSRA & (1 << ADSC));    
  return ADC;                      
}

void setup() {
  Serial.begin(9600);
  pinMode(pinRawDC, INPUT);
  pinMode(pinPulseAC, INPUT);
  analogReference(DEFAULT);
  Tlc.init();
  LEDsoff();
  calibrate();
  Serial.println("Time, Ambient DC, Ambient AC, I Active 740, Live Pulse 740, I Active 850, Live Pulse 850, mBLL [mMoles], I Base 740, I Base 850");
  Serial.print(millis());
  Serial.print(" , ");
  Serial.print("0, 0, 0, 0, 0, 0, 0, ");
  Serial.print(I_base740);
  Serial.print(" , ");
  Serial.println(I_base850);

  // Configure Timer 1 for a precise 4000 Hz (250 microseconds) Interrupt
  cli();                      // Disable global interrupts
  TCCR1A = 0;                 // Set entire TCCR1A register to 0
  TCCR1B = 0;                 // Set entire TCCR1B register to 0
  TCNT1  = 0;                 // Initialize counter value to 0
  
  // Set compare match register for 4000 Hz increment
  // Formula: (16,000,000 Hz / (Prescaler * Desired Frequency)) - 1
  // (16000000 / (1 * 4000)) - 1 = 3999
  OCR1A = 15999;             
  
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
  int currentDC = fastAnalogRead(pinRawDC);
  int currentAC = fastAnalogRead(pinPulseAC);
  switch(sample_index){
    case 0: 
    case 1:
      ambientDC = currentDC;
      ambientAC = currentAC;
      break;

    case 2:
    case 3:
      I_Active740 = currentDC - ambientDC; 
      livePulseAC740 = currentAC - ambientAC;
      I_Active850 = 0;
      livePulseAC850 = 0;
      break;

    case 4:
    case 5:
      ambientDC = currentDC;
      ambientAC = currentAC;
      break;

    case 6:
    case 7:
      I_Active850 = currentDC - ambientDC; 
      livePulseAC850 = currentAC - ambientAC;
      I_Active740 = 0;
      livePulseAC740 = 0;
      break;
  }

 
  /*int raw_adc = analogRead(ANALOG_PIN); 

  // 2. Lock-In Demodulation (Multiply signal by synchronized reference)
  long multiplied = (long)raw_adc * ref_sine[sample_index];

  // 3. Digital Low-Pass Filter (Exponential Moving Average using Fixed-Point Math)
  // Formula: filtered = (ALPHA * multiplied + (1000 - ALPHA) * last_filtered) / 1000
  lockin_output = (ALPHA * multiplied + (1000 - ALPHA) * lockin_output) / 1000;
*/
  // 4. Advance sample index (loops 0 to 7)
  // Optional: Toggle your LED pin here if you want the Mega to handle the 500Hz square-wave generation
  // (e.g., Turn ON if sample_index < 4, Turn OFF if sample_index >= 4)
  
  sample_index = (sample_index + 1) % 8;
  if (sample_index == 0 || sample_index == 4){
    LEDsoff();
  }
  else if(sample_index == 1){
    updateLED1(DEOXY_740NM, BRIGHT740);     //740nm LED active
  }
  else if(sample_index == 5){
    updateLED1(OXY_850NM, BRIGHT850);       //850nm LED active
  }

}

void loop() {
  // Print out the extracted DC hemodynamic signal every 50ms
  static unsigned long last_print = 0;
  if (millis() - last_print > 250) {
    last_print = millis();
    
    // Read the volatile variable safely by temporarily halting interrupts
    cli();
    //long print_val = lockin_output;
    int DC = ambientDC;
    int AC = ambientAC;
    float I_A740 = I_Active740;
    int live740 = livePulseAC740;
    float I_A850 = I_Active850;
    int live850 = livePulseAC850;
    sei();

    // Out value represents the isolated absorption data at 500 Hz
    //Serial.println(print_val); 
    printSample(DC, AC, I_A740, live740, I_A850, live850, 0);
  }
}