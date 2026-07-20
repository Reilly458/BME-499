#include "SparkFun_Tlc5940.h"

//TLC5940 LED Driver Setup
#define DEOXY_740NM  1  //blue
#define OXY_850NM  2     //red
#define BRIGHT740  4095
#define BRIGHT850  4095 // decrease to 3276 to balance brightness
#define CAL_NUM 20       // number of readings to take for calibration
#define CAL_DELAY 15   // 300/CAL_NUM, 300 seconds divided by number of readings, 300 chosen since max time led is turn on is 250ms for 1Hz

// Control Pins
//#define XLAT_PIN  PB5             // Define XLAT (Latch) pin on Port B pin 5
//#define BLANK_PIN PB6             // Define BLANK (PWM reset) pin on Port B pin 6

// Global Variables
//uint16_t led_data[16] = {0};      // Array holding 12-bit brightness values for TLC channels
volatile uint16_t ambientDC = 0;
volatile uint16_t ambientAC = 0;
volatile uint16_t adc_a0 = 0;     // Stores the latest raw A0 analog reading 740 ambient DC/ I active
volatile uint16_t adc_a1 = 0;     // Stores the latest raw A1 analog reading 850 ambient DC/ I active
volatile uint16_t adc_a2 = 0;     // Stores the latest raw A2 analog reading 740 ambient AC / livePulseAC
volatile uint16_t adc_a3 = 0;     // Stores the latest raw A3 analog reading 850 ambient AC / livePulseAC
volatile uint8_t state = 0;       // Tracking variable for the 4 LED states (0 to 3)
volatile uint8_t time_ticks = 0;  // Counter incremented every 50ms for timing
volatile uint8_t data_ready = 0;  // Flag set to 1 when a new ADC sample is taken

//Timing
const int flash_ms = 1000;    //15ms ~ 16Hz sweep rate

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
  //LEDsoff();
  //delay(2);
  
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

  //if(TLC_channel == 1){ 
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
  //}
   LEDsoff();
   delay(1000);
   updateLED1(2, brightness);
   delay(25);
  //else{
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
  //}
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
  //delay(1000);
  //getBaseline(OXY_850NM, BRIGHT850);
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

void printSample(/*int ambientDC, int ambientAC, */float active740, float active850, int lpulse740,  int lpulse850/*, float mBLL*/){
  Serial.print(millis());
  Serial.print(",");
 /* Serial.print(ambientDC);
  Serial.print(",");
  Serial.print(ambientAC);
  Serial.print(",");*/
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

/*
void spi_init(void) {
	DDRB |= (1 << DDB2) | (1 << DDB1) | (1 << XLAT_PIN) | (1 << BLANK_PIN); // Set SPI pins as output
	DDRB |= (1 << DDB0);                 // Keep SS pin as output to lock Master Mode
	SPCR = (1 << SPE) | (1 << MSTR);        // Enable SPI in Master Mode
}

void spi_transmit(uint8_t data) {
	SPDR = data;                         // Start 8-bit hardware transmission
	while (!(SPSR & (1 << SPIF)));        // Wait until transmission cycle finishes
}

void send_tlc_data(void) {
	PORTB |= (1 << BLANK_PIN);           // Disable TLC outputs during shifting
	for (int i = 15; i > 0; i -= 2) {
		uint16_t ch_a = led_data[i];     // Odd channel
		uint16_t ch_b = led_data[i-1];   // Even channel
		spi_transmit((ch_a >> 4) & 0xFF);
		spi_transmit(((ch_a & 0x0F) << 4) | ((ch_b >> 8) & 0x0F));
		spi_transmit(ch_b & 0xFF);
	}
	PORTB |= (1 << XLAT_PIN);            // Pulse latch high
	PORTB &= ~(1 << XLAT_PIN);           // Pull latch low
	PORTB &= ~(1 << BLANK_PIN);          // Re-enable PWM driving
}

void timer1_init(void) {
	DDRB |= (1 << DDB5);                 // Set PB5 (OC1A) as physical output for GSCLK
	TCCR1A = (1 << COM1A0);              // Toggle OC1A on compare match
	TCCR1B = (1 << WGM12) | (1 << CS10); // Set CTC mode, no prescaler
	OCR1A = 3;                           // Broadcast background 2MHz clock signal
}*/

void adc_init(void) {
	 /*
	ADMUX - ADC Multiplexer Selection Register	
	bit          7           6          5         4        3         2          1          0
	name       REFS1       REFS0      ADLAR       -       MUX3      MUX2       MUX1       MUX0
	set to       0           1          0         0        0         0          0          0
	
	REFS1 = 0    
	REFS0 = 1	use AVCC for reference voltage
	ADLAR = 0    right justify ADC result in ADCH/ADCL	
	bit 4 = 0	
	MUX3 = 0     set dynamically, pin A0 = 0x00 
	MUX2 = 0
	MUX1 = 0
	MUX0 = 0
	*/
	ADMUX = (1 << REFS0);                // Set AVCC (5V) as reference voltage
	/*ADCSRA - ADC Control and Status Register A
	bit          7           6            5          4          3            2           1           0
	name        ADEN        ADSC        ADATE       ADIF       ADIE        ADPS2       ADPS1       ADPS0
	set to       1           0            0          0          0            1           1           1	
	
	ADEN = 1     enable ADC
	ADSC = 0     don't start ADC yet
	ADATE = 0    disable ADC auto trigger 
	ADIF = 0     don't set ADC interrupt flag
	ADIE = 0     disable ADC interrupt	
	ADPS2 = 1
	ADPS1 = 1    Prescaler = 128. Calculates out to: 16 MHz CPU clock / 128 = 125 kHz ADC hardware clock).
	ADPS0 = 1
	*/
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Enable ADC, prescaler 128
}

uint16_t adc_read(uint8_t channel) {
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F); // Select analog input channel, current ADMUX 0b10000000 bitwise or (channel # 0b0000cccc bitwise and 0b00001111)
	ADCSRA |= (1 << ADSC);               // Start conversion, targets only ADSC bit position(6) and sets it to 1, other positions remain unchanged
	while (ADCSRA & (1 << ADSC));        // Wait for hardware to clear flag, runs until ADSC bit is set back to 0
	return ADC;                          // Return 10-bit numerical result (0-1023)
}

void timer3_init(void) {
	/*
	TCCR3A - Timer/Counter 3 Control Register A
	bit           7         6         5         4        3       2        1        0
	name        COM3A1    COM3A0    C3M0B1    COM3B0     -       -      WGM31    WGM30
	set to        0         0         0         0        0       0        0        0
	
	COM3A1 = 0    normal port operation, OC0A disconnected
	COM3A0 = 0	
	COM3B1 = 0    normal port operation, OC0B disconnected
	COM3B0 = 0	
	bit 3 = 0
	bit 2 = 0	
	WGM31 = 0     CTC (Clear Timer on Compare match) mode, see TCCR3B also
	WGM30 = 0     TCNT3 will count up to value in OCR3A, then signal timer 3 compare interrupt
	*/
	TCCR3A = 0;     // Reset control register
	/*
	TCCR3B - Timer/Counter 3 Control Register B	
	bit           7          6        5       4         3         2         1        0
	name        FOC3A      FOC3B      -       -       WGM32      CS32      CS31     CS30
	set to        0          0        0       0         1         1         0        0
	
	FOC3A = 0     don't use Force Output Compare A
	FOC3B = 0
	bit 5 = 0
	bit 4 = 0
	WGM32 = 1     CTC (Clear Timer on Compare match) mode 4
	CS32 = 1	  clock / 256
	CS31 = 0      clock / 1024
	CS30 = 0
	*/
	TCCR3B = (1 << WGM32) | (1 << CS32); // Set CTC mode, prescaler 256
	OCR3A = 3124;                        // Set threshold for exactly 50ms intervals, Set count limit: (16000000Hz / 256 clock speed) * 0.050 seconds - 1 = 3124 ticks
	/*
	TIMSK3 - Timer/Counter 3 Interrupt Mask Register	
	bit           7        6        5       4       3       2         1         0
	name          -        -        -       -       -     OCIE3B    OCIE3A    TOIE3
	set to        0        0        0       0       0       0         1         0
	
	bit 7 = 0     don't use Force Output Compare A
	bit 6 = 0
	bit 5 = 0
	bit 4 = 0
	bit 3 = 0
	OCIE3B = 0    don't enable Timer/Counter 3 Output Compare Match B Interrupt
	OCIE3A = 1    enable Timer/Counter 3 Output Compare Match A Interrupt Enable
	TOIE3 = 0     don't enable Timer/Counter 3 Overflow Interrupt
	*/
	TIMSK3 |= (1 << OCIE3A);             // Enable Timer 3 match interrupt
}

/*void clear_all_leds(void) {
	for (int i = 0; i < 16; i++) {
		led_data[i] = 0;                 // Set channel brightness to zero
	}
}*/

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

	// Step sequencer block checking if 250ms total window has elapsed
	if (time_ticks >= 5) {
		time_ticks = 0;                  // Reset tick timer
		state = (state + 1) % 4;                         // Step to next scene scenario
		//if (state > 3) state = 0;        // Loop back to start if array overflows
		switch(state){
		//if (state == 0 || state == 2) {
			case 0:
			case 2:
				LEDsoff();            // Drop both outputs to 0% PWM
				break;
		//}
		//else if (state == 1) {
			case 1:
				//clear_all_leds();
				//led_data[1] = 4095;          // Turn LED 1 to 100% full scale
        updateLED1(DEOXY_740NM, BRIGHT740);
				break;
		//}
		//else if (state == 3) {
			case 3:
				//clear_all_leds();
				//led_data[2] = 4095;          // Turn LED 2 to 100% full scale
        updateLED1(OXY_850NM, BRIGHT850);
				break;
		//}
		}
		//send_tlc_data();                 // Transmit array profile to physical chips
	}
}

void setup() {
	//spi_init();                          // Initialize SPI buses
	//timer1_init();                       // Start background GSCLK engine
	
  Serial.begin(115200);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  analogReference(DEFAULT);
  Tlc.init();
  LEDsoff();
  adc_init();                          // Start ADC system
  calibrate();
  
 // cli();                               // Block global interrupts during timer setup
	timer3_init();                       // Run 50ms step clock
	//usart_init(MYUBRR);                  // Open serial communication port at 9600 baud
	
	sei();                               // Globally enable interrupts
}
	//char tx_buffer[64];                  // Local string buffer memory for text building
  //sprintf(tx_buffer, "Time, adc0, adc1, adc2, adc3, state\r\n");
  //usart_print_string(tx_buffer); 

void loop() {
		if (data_ready) {                // Check if background timer completed a sample
			data_ready = 0;              // Reset flag immediately to catch next pass
			
			// Format text showing current running State, A0 value, and A1 value
			//sprintf(tx_buffer, "%lu,%u,%u,%u,%u,%d\r\n", (unsigned long)millis(), adc_a0, adc_a1, adc_a2, adc_a3, state);
			//usart_print_string(tx_buffer); // Push text string out over USB port
      printSample(adc_a0, adc_a1, adc_a2, adc_a3);
      Serial.println();
		}
	
}
