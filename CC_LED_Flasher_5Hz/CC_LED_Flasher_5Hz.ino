#include "SparkFun_Tlc5940.h"

//TLC5940 LED Driver Setup
#define DEOXY_740NM  1  //blue
#define OXY_850NM  2     //red
#define BRIGHT740  4095
#define BRIGHT850  4095 // decrease to 3276 to balance brightness
#define CAL_NUM 20       // number of readings to take for calibration
#define CAL_DELAY 15   // 300/CAL_NUM, 300 seconds divided by number of readings, 300 chosen since max time led is turn on is 250ms for 1Hz
#define BUTN_OLFACTO 21
#define BUTN_THERAPY 20
#define OLFACTO 34      // Pseudo olfactometer mode
#define THERAPY 35      // Psesudo therapy mode

// Global Variables
volatile uint16_t ambientDC = 0;
volatile uint16_t ambientAC = 0;
volatile uint16_t adc_a0 = 0;     // Stores the latest raw A0 analog reading 740 ambient DC/ I active
volatile uint16_t adc_a1 = 0;     // Stores the latest raw A1 analog reading 850 ambient DC/ I active
volatile uint16_t adc_a2 = 0;     // Stores the latest raw A2 analog reading 740 ambient AC / livePulseAC
volatile uint16_t adc_a3 = 0;     // Stores the latest raw A3 analog reading 850 ambient AC / livePulseAC
volatile uint8_t state = 0;       // Tracking variable for the 4 LED states (0 to 3)
volatile uint8_t time_ticks = 0;  // Counter incremented every 50ms for timing
volatile uint8_t data_ready = 0;  // Flag set to 1 when a new ADC sample is taken

//mBLL Variables
//float DPF = 3.5;    //~3.5-4 fpr mucous membrane tissue
//const float r = 0.0;    //distance between center of led and center of photodiode, mm or um most likely
//const float L_DPF = r * DPF; 
//Delta_hbo2 = COD2 - DOD1  // C = 3.5671E-01, D = -2.2734E-03
//Delta_hhb = AOD2 - BOD1   // A = 4.2018E-01, B = 9.9528E-04
float I_base740 = 0.0;
float I_base850 = 0.0;
volatile uint16_t active740 = 0.0;     // Stores the latest averaged A0 analog reading 740 ambient DC/ I active
volatile uint16_t active850 = 0.0;     // Stores the latest averaged A1 analog reading 850 ambient DC/ I active
volatile uint16_t live740 = 0.0;       // Stores the latest averaged A2 analog reading 740 ambient AC / livePulseAC
volatile uint16_t live850 = 0.0;       // Stores the latest averaged A3 analog reading 850 ambient AC / livePulseAC

//Button States add pause and stop buttons?
volatile int olfacto_state = 0;
volatile int therapy_state = 0;

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
  for(int i = 0; i < CAL_NUM; i++){
    int baseDC = adc_read(0);
    ambientDC += baseDC;    //Measure dark ambient light with both LEDs off, raw ADC value
    int baseAC = adc_read(2);
    ambientAC += baseAC;
    /*Serial.print("Ambient DC = ");
    Serial.print(baseDC);
    Serial.print("  |  Ambient AC = ");
    Serial.println(baseAC);*/
    delay(CAL_DELAY);
  }
  ambientDC /= CAL_NUM;
  ambientAC /= CAL_NUM;
  Serial.print("Ambient DC = ");
  Serial.print(ambientDC);
  Serial.print("  |  Ambient AC = ");
  Serial.println(ambientAC);

  int base = 0;
  updateLED1(TLC_channel, brightness);    //Turn on 1 LED, 15ms delay for opamp settling
  delay(25);

  Serial.print("740nm Baseline readings: ");
  for(int i = 0; i < CAL_NUM; i++){
    base = adc_read(0) - ambientDC;   //Read LED on signal and subtract off dark ADC value
    I_base740 += base;    
    Serial.print(base);
    Serial.print(", ");
    delay(CAL_DELAY);
  }
  I_base740 /= CAL_NUM;
  Serial.println(I_base740);

  LEDsoff();
  delay(1000);

  updateLED1(2, brightness);
  delay(25);
  Serial.print("850nm Baseline readings: ");
  for(int i = 0; i < CAL_NUM; i++){
    base = adc_read(1) - ambientDC;   //Read LED on signal and subtract off dark ADC value
    I_base850 += base;    
    Serial.print(base);
    Serial.print(", ");
    delay(CAL_DELAY);
  }
  I_base850 /= CAL_NUM;
  Serial.println(I_base850);  
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

  // Safety check to ensure the probe isn't reading open air/0 light
  if (I_base740 < 1.0 || I_base850 < 1.0) {
    Serial.println("ERROR: Low baseline signal detected! Check probe placement and restart Arduino.");
    while(1); // Halt execution
  }
  Serial.println("Calibration Complete.");
  delay(1000);
  Serial.println("Time, 740 I Active, 850 I Active, 740 Live Pulse, 850 Live Pulse, mBLL [mMoles], Ambient DC, Ambient AC, 740 I Base, 850 I Base");
  Serial.print(millis());
  Serial.print(", ");
  Serial.print("0, 0, 0, 0, 0, ");
  Serial.print(ambientDC);
  Serial.print(", ");
  Serial.print(ambientAC);
  Serial.print(", ");
  Serial.print(I_base740);
  Serial.print(", ");
  Serial.println(I_base850);
}

void printSample(float active740, float active850, int lpulse740,  int lpulse850/*, float mBLL*/){
  Serial.print(millis());
  Serial.print(",");
  Serial.print(active740);
  Serial.print(",");
  Serial.print(active850);
  Serial.print(",");
  Serial.print(lpulse740);
  Serial.print(",");
  Serial.print(lpulse850);
 // Serial.print(",");
 // Serial.println(mBLL); 
}

void adc_init(void) {
	 /* ADMUX - ADC Multiplexer Selection Register	
	bit          7           6          5         4        3         2          1          0
	name       REFS1       REFS0      ADLAR      MUX4     MUX3      MUX2       MUX1       MUX0
	set to       0           1          0         0        0         0          0          0
	
	REFS1 = 0    
	REFS0 = 1	   use AVCC for reference voltage, capacitor at AREF to GND
	ADLAR = 0    right justify ADC result in ADCH/ADCL; write 1 to left justify	
	MUX4 = 0	
	MUX3 = 0     set dynamically, pin A0 = 0x00 
	MUX2 = 0
	MUX1 = 0
	MUX0 = 0  */
	ADMUX = (1 << REFS0);     // Set AVCC (5V) as reference voltage

	/*  ADCSRA - ADC Control and Status Register A
	bit          7           6            5          4          3            2           1           0
	name        ADEN        ADSC        ADATE       ADIF       ADIE        ADPS2       ADPS1       ADPS0
	set to       1           0            0          0          0            1           1           1	
	
	ADEN = 1     enable ADC, 1 on 0 off
	ADSC = 0     don't start ADC yet, 1 starts conversion
	ADATE = 0    disable ADC auto trigger 
	ADIF = 0     don't set ADC interrupt flag
	ADIE = 0     disable ADC interrupt	
	ADPS2 = 1    Prescalar = 16
	ADPS1 = 1    Prescaler = 4  
	ADPS0 = 1    Prescaler = 2  
  Want ADC clock speed between 50kHz-200kHz for ideal 10 bit resolution
  Calculates out to: CPU clock / prescale = ADC hardware clock  */
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Enable ADC, prescaler 2 x 4 x 16 = 128 Calculates out to: 16 MHz CPU clock / 128 = 125 kHz ADC hardware clock). 
  // ADC takes 13 clock cycles to read, 13/125kHz = 104us per reading x 5 = 520us per 5 readings < 50ms per phase for 5Hz
}

uint16_t adc_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F); // Select analog input channel, current ADMUX 0b10000000 bitwise or (channel # 0b0000cccc bitwise and 0b00001111)
	ADCSRA |= (1 << ADSC);               // Start conversion, targets only ADSC bit position(6) and sets it to 1, other positions remain unchanged
	while (ADCSRA & (1 << ADSC));        // Wait for hardware to clear flag, runs until ADSC bit is set back to 0
	return ADC;                          // Return 10-bit numerical result (0-1023)
}

void timer3_init(void) {
	/*  TCCR3A - Timer/Counter 3 Control Register A
	bit           7         6         5         4        3        2        1        0
	name        COM3A1    COM3A0    C3M0B1    COM3B0   COM3C1   COM3C0   WGM31    WGM30
	set to        0         0         0         0        0        0        0        0
	
	COM3A1 = 0    Output compare channel A, normal port operation, OC0A disconnected
	COM3A0 = 0	  
	COM3B1 = 0    Output compare channel B, normal port operation, OC0B disconnected
	COM3B0 = 0	
	COM3C1 = 0    Output compare channel C, normal port operation, OC0C disconnected
  COM3C0 = 0 
	bit 2 = 0	
	WGM31 = 0     CTC (Clear Timer on Compare match) mode, see TCCR3B also
	WGM30 = 0     TCNT3 will count up to value in OCR3A, then signal timer 3 compare interrupt  */
	TCCR3A = 0;     // Reset control register

	/*  TCCR3B - Timer/Counter 3 Control Register B	
	bit           7          6        5       4         3         2         1        0
	name        ICNC3      ICES3      -     WGM33     WGM32      CS32      CS31     CS30
	set to        0          0        0       0         1         0         1        0
	
	ICNC3 = 0     Input capture noise canceler
	ICES3 = 0     Input capture edge select
	bit 5 = 0     Reserved bit
	WGM33 = 0     Waveformgeneration mode
	WGM32 = 1     Waveform generation mode, CTC (Clear Timer on Compare match) mode 4
	CS32 = 0	    clock / 256
	CS31 = 1      clock / 8
	CS30 = 0      clock / 1 
  [001] = 1; [010] = 8; [011]= 64, [100] = 256, [101] = 1024
  max delay = ( max timer steps x prescale) clock freq, 16 bit timer gives 2^16 = 65536 steps
  max delay = (65536 x 1) /16000000Hz = 4.09ms < 10ms
  max delay = (65536 x 8) /16000000Hz = 32.7ms > 10ms */
	TCCR3B = (1 << WGM32) | (1 << CS31)  ; // Set CTC mode, prescaler 8
	OCR3A = 19999; // Set threshold for exactly 10ms intervals, Set count limit: 16000000Hz / (8 clock speed * 100Hz(timer freq))  - 1 = 19999 ticks

	/*  TIMSK3 - Timer/Counter 3 Interrupt Mask Register	
	bit           7        6        5       4       3       2         1         0
	name          -        -      ICIE3     -     OCIE3C  OCIE3B    OCIE3A    TOIE3
	set to        0        0        0       0       0       0         1         0
	
	bit 7 = 0     
	bit 6 = 0
	ICIE5 = 0     Timer/Counter 3 Input Capture Interupt Enable
	bit 4 = 0
	OCIE3C = 0    Timer/Counter 3 Output Compare Match C Interrupt Enable
	OCIE3B = 0    Timer/Counter 3 Output Compare Match B Interrupt Enable
	OCIE3A = 1    Timer/Counter 3 Output Compare Match A Interrupt Enable
	TOIE3 = 0     Timer/Counter 3 Overflow Interrupt  */
	TIMSK3 |= (1 << OCIE3A);             // Enable Timer 3 match interrupt
}

// Background worker running every 50ms
ISR(TIMER3_COMPA_vect) {
	time_ticks++;                        // Increment timeline counter
	
	// Read appropriate channels based on matching active lighting phase
	if (state == 0 || state == 2) {
		adc_a0 = adc_read(0);            // Read A0 (Ambient DC Check) 
    adc_a1 = adc_read(1);
		adc_a2 = adc_read(2);            // Read A2 (Ambient AC Check)
    adc_a3 = adc_read(3);
	}
	else if (state == 1) {
		adc_a0 = adc_read(0)-ambientDC;            // Read A0 (LED 1 Active DC) 740nm
		adc_a2 = adc_read(2)-ambientAC;            // Read A2 (LED 1 livePulseAC) 740nm
	}
	else if (state == 3) {
		adc_a1 = adc_read(1)-ambientDC;            // Read A1 (LED 2 Active DC)  850nm
		adc_a3 = adc_read(3)-ambientAC;	      		 // Read A3 (LED 2 livePulseAC) 850nm
	}

	data_ready = 1;                      // Set flag telling main loop new numbers are ready to print

	// Step sequencer block checking if 50ms total window has elapsed
	if (time_ticks >= 5) {
		time_ticks = 0;                  // Reset tick timer
		state = (state + 1) % 4;         // Step to next scene scenario
		switch(state){
			case 0:
			case 2:
				LEDsoff();            // Drop both outputs to 0% PWM
				break;
			case 1:
        updateLED1(DEOXY_740NM, BRIGHT740);
				break;
			case 3:
        updateLED1(OXY_850NM, BRIGHT850);
				break;
		}
	}
}

//Olfactometer mode button
ISR(INT0_vect){
  olfacto_state = 1;
}

//Therapy mode button
ISR(INT1_vect){
  therapy_state = 1;
}

void setup() {
  Serial.begin(115200);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(BUTN_OLFACTO, INPUT);
  pinMode(BUTN_THERAPY, INPUT);
  pinMode(OLFACTO, OUTPUT);
  pinMode(THERAPY, OUTPUT);
  analogReference(DEFAULT);
  Tlc.init();
  LEDsoff();
  adc_init();                          // Start ADC system
  calibrate();
  timer3_init();                       // Run 10ms step clock
  EIFR = (1 << INTF1) | (1 << INTF0);   // Clear any ghost flags thrown during the calibration delays
  EICRA |= (1 << ISC11) | (1 << ISC10) | (1 << ISC01) | (1 << ISC00);     // Set INT0 and INT1 to trigger on rising edge
  EIMSK |= (1 << INT1) | (1 << INT0);   // Enable INT0 and INT1 interupts 
	sei();                               // Globally enable interrupts
}

void loop() {
  if(olfacto_state){
    digitalWrite(OLFACTO, !digitalRead(OLFACTO));
    olfacto_state = 0;
  }
  if(therapy_state){
    digitalWrite(THERAPY, !digitalRead(THERAPY));
    therapy_state = 0;
  }
	if (data_ready) {              // Check if background timer completed a sample
		data_ready = 0;              // Reset flag immediately to catch next pass			
    printSample(adc_a0, adc_a1, adc_a2, adc_a3);
    Serial.println();
	}
}
