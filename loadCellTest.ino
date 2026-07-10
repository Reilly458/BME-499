#include <LiquidCrystal.h>

#include <HX711.h>
#include <EEPROM.h>

#include <SoftwareSerial.h>
#include <Tic.h>

//Change Rig Specific Settings
//******************************************************************************************************

//boolean settings 1
#define SET_BOOLEAN_SETTINGS_1 true //set to true when updating settings.

//*warning* Currently set up for low force rig  
//All settings from boolean 1 are set at the same time, so ensure they are all set correctly
#define IS_TWO_LIMIT_SWITCH false
#define IS_FORWARD_LEFT true
#define IS_TIC_MICROSTEP_MAX_32 false
#define DISABLE_ACTUATOR_CONTROL true
#define IS_LOW_RESOLUTION true


//max force
#define SETMAXFORCE true //set to true when updating max force.
#define NEWMAXFORCE 600


uint8_t bool_setting_var_1;
//boolean variables list (with index)
bool is_two_limit_switch;//(BOOLEAN_SETTING_VARIABLES_1 & 1)
bool is_forward_left; //(BOOLEAN_SETTING_VARIABLES_1 & 2)
bool is_tic_microstep_max_32; //(BOOLEAN_SETTING_VARIABLES_1 & 4)
bool disable_actuator_control; //(BOOLEAN_SETTING_VARIABLES_1 & 8)
bool is_low_resolution; //(BOOLEAN_SETTING_VARIABLES_1 & 16)

int16_t rig_max_force;



/*
Function Declarations
*/
//assorted functions
int move(long int steps);
void runTest();
int cycle(int size , int position , int step); //used for looping menus


//Mode functions
int main_menu(int exit);

int hardware_menu(int exit);
int stepper_menu(int exit);
int actuator_menu(int exit);
int calibration_menu(int exit);

int test_menu(int exit);
int model_menu(int exit);
int mode_menu(int exit);
int slow_menu(int exit);

//test parameters
const long test_speed = 2000000;
const long max_speed = 64000000;
const int stop_time = 4000;


//model control
int model = 0;
int model_count = 8; //include null selection
String models[] = {
  "     ",
  "P8   ",
  "P16  ",
  "PQ12 ",
  "L8   ",
  "L12  ",
  "L16  ",
  "S20  "
};
//LCD pins
//Reilly changes: d3=3 -> d3=10, d2=2 -> d2=13 to make Arduino D2/D3 available for encoder
const int rs = 7, en = 6, d5 = 5, d4 = 4, d3 = 10, d2 = 13; 
//HX711 pins
const int LOADCELL_DOUT_PIN = 8, LOADCELL_SCK_PIN = 9;
const int d12 = 12, d6 = 6;
int motorSpeed = 0, initPos = 0, pos = 0;
//LCD reading
double reading = 0;
LiquidCrystal lcd(rs, en, d5, d4, d3, d2);
HX711 scale;
int force_calibration_constant;

//stepper
TicI2C stepper;

//mode control
int mode = 0;
int mode_cursor = 0;

bool fast_mode = false;
int fast_mode_cursor = 0;
const int peaks_per_skip = 5;
int32_t skip_distance = 0;

int slow_cursor = 0;


int max_force_override = 0;
int32_t compressive_distance = 0;
int compressive_cursor = 0;
int early_reading_mode = 0;


//define custom LCD characters (6x8 grid, define row by row)
//left (0)
byte left[8] = {
	0x00,0x04,0x08,0x1f,0x08,0x04,0x00,0x00
};
/*
2 1|8 4 2 1
-----------
. .|. . . . (0x00)
. .|. X . . (0x04)
. .|X . . . (0x08)
. X|X X X X (0x1f)
. .|X . . . (0x08)
. .|. X . . (0x04)
. .|. . . . (0x00)
. .|. . . . (0x00)
*/

//pause (1)
byte pause[8] = {
	0x00,0x1b,0x1b,0x1b,0x1b,0x1b,0x00,0x00
};
/*
2 1|8 4 2 1
-----------
. .|. . . . (0x00)
. X|X . X X (0x1b)
. X|X . X X (0x1b)
. X|X . X X (0x1b)
. X|X . X X (0x1b)
. X|X . X X (0x1b)
. .|. . . . (0x00)
. .|. . . . (0x00)
*/

//right (2)
byte right[8] = {
  0x00,0x04,0x02,0x1f,0x02,0x04,0x00,0x00
};
/*
2 1|8 4 2 1
-----------
. .|. . . . (0x00)
. .|. X . . (0x04)
. .|. . X . (0x02)
. X|X X X X (0x1f)
. .|. . X . (0x02)
. .|. X . . (0x04)
. .|. . . . (0x00)
. .|. . . . (0x00)
*/

//stop (3)
byte stop[8] = {
  0x00,0x1f,0x1f,0x1f,0x1f,0x1f,0x00,0x00
};
/*
2 1|8 4 2 1
-----------
. .|. . . . (0x00)
. X|X X X X (0x1f)
. X|X X X X (0x1f)
. X|X X X X (0x1f)
. X|X X X X (0x1f)
. X|X X X X (0x1f)
. .|. . . . (0x00)
. .|. . . . (0x00)
*/

//Encoder settings
int encoder0PinA = 2;
int encoder0PinB = 3;
volatile int encoder0Pos = 0;
volatile int encoder0PinALast = LOW;
volatile int a = LOW;
volatile int b = LOW;

// Delays for the specified number of milliseconds while
// resetting the Tic's command timeout so that its movement does
// not get interrupted by errors.
void delayWhileResettingCommandTimeout(uint32_t ms)
{
  uint32_t start = millis();
  do
  {
    stepper.resetCommandTimeout();
  } while ((uint32_t)(millis() - start) <= ms);
}
void delayWhileResettingCommandTimeoutandObservingLimits(uint32_t ms)
{
  uint32_t start = millis();
  do
  {
    stepper.resetCommandTimeout();
    if( (stepper.getCurrentPosition()<=0 ||(is_two_limit_switch && digitalRead(A6))) && stepper.getTargetVelocity()<0){
      stepper.setTargetVelocity(0);
      stepper.haltAndHold();
    }
  } while ((uint32_t)(millis() - start) <= ms);
}


/*
Setup
*****************************************************
*/
void setup() {
  //LCD setup
  //*********************
  lcd.clear();
  lcd.begin(16, 2);
  lcd.createChar(0,left);
  lcd.createChar(1,pause);
  lcd.createChar(2,right);
  lcd.createChar(3,stop);

  Serial.begin(115200);
  //initialize load cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  //Memory setup
  //*********************
  /*
  EEPROM allocation

  0 -> scale calibration
  1 -> scale calibration

  2 -> boolean settings 1

  3 -> max force
  4 -> max force


  **note 
  uint8_t and char use 1 byte (and int8_t, but don't use that)
  uint16_t, int16_t and int use 2 bytes

  boolean values should always be part of a boolean set, generally stored as a set of 8 in a uint8_t
  */


  EEPROM.get(0,force_calibration_constant); //read calibration from memory
  scale.set_scale(force_calibration_constant);  //calibration factor = uncalibrated mass reading on load cell/actual mass of object
  //scale.tare();  moved to after user confirms 0 load

//Get all machine specific settings
//  |= to set a specific bit, & to read a specific bit (each bit is at a power of 2)
  if(SET_BOOLEAN_SETTINGS_1){
    bool_setting_var_1 = 0; //initialize setting as 0

    //set each of the 8 bits according to if statements. 
    if(IS_TWO_LIMIT_SWITCH) bool_setting_var_1 |= 1;
    if(IS_FORWARD_LEFT) bool_setting_var_1 |= 2;
    if(IS_TIC_MICROSTEP_MAX_32) bool_setting_var_1 |= 4;
    if(DISABLE_ACTUATOR_CONTROL) bool_setting_var_1 |= 8;
    if(IS_LOW_RESOLUTION) bool_setting_var_1 |= 16;
    //if() bool_setting_var_1 |= 32;
    //if() bool_setting_var_1 |= 64;
    //if() bool_setting_var_1 |= 128;
    EEPROM.put(2,bool_setting_var_1);
  } else{
    EEPROM.get(2,bool_setting_var_1);
  }
  if(SETMAXFORCE){
    EEPROM.put(3,(int16_t)NEWMAXFORCE);
    rig_max_force = NEWMAXFORCE;
  } else{
    EEPROM.get(3,rig_max_force);
  }


  is_two_limit_switch = (bool_setting_var_1 & 1)? true:false;
  is_forward_left = (bool_setting_var_1 & 2)? true:false;
  is_tic_microstep_max_32 = (bool_setting_var_1 & 4)? true:false;
  disable_actuator_control = (bool_setting_var_1 & 8)? true:false;
  is_low_resolution = (bool_setting_var_1 & 16)? true:false;
  //() = (bool_setting_var_1 & 32)? true:false;
  //() = (bool_setting_var_1 & 64)? true:false;
  //() = (bool_setting_var_1 & 128)? true:false;



  lcd.clear();
  Serial.flush();

  //Encoder setup
  //*********************
  pinMode (encoder0PinA,INPUT_PULLUP); 
  pinMode (encoder0PinB, INPUT_PULLUP);
  attachInterrupt(1, CountA, CHANGE);

  
  //Actuator setup
  //*********************
  pinMode(11,OUTPUT);
  pinMode(12,OUTPUT);

  //Stepper setup
  //*********************
  // Set up I2C.
  Wire.begin();

  // Give the Tic some time to start up.
  delayWhileResettingCommandTimeout(50);
  stepper.exitSafeStart();
  stepper.setStepMode(TicStepMode::Microstep8);
  stepper.setMaxSpeed(max_speed);
  //stop stepper
  move(0);
  


  //User setup
  //*********************
  //wait to zero force input
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("zero force?");
  stepper.energize();
  int dir = (is_forward_left)? -1:1;
  while (!digitalRead(A1))
  {
    if(digitalRead(A0)){
      move(-stepper.getMaxSpeed()*dir);
      lcd.setCursor(15,0);
      lcd.write((uint8_t)0);
    } else if(digitalRead(A2)){
      move(stepper.getMaxSpeed()*dir);
      lcd.setCursor(15,0);
      lcd.write((uint8_t)2);
    } else{
      move(0);
      lcd.setCursor(15,0);
      lcd.write((uint8_t)1);
    }
    delayWhileResettingCommandTimeout(1);
  }

  //reset for waiting
  scale.tare();//set 0 on loadcell
  lcd.clear();
  move(0);//reset target stepper velocity
  stepper.deenergize();//disable stepper
  digitalWrite(11,LOW);//disable actuator signal (should already be low)
  digitalWrite(12,LOW);
  delayWhileResettingCommandTimeout(30);//debounce
  while(digitalRead(A0)||digitalRead(A1)||digitalRead(A2));//wait for no pressed buttons
  delayWhileResettingCommandTimeout(30);//debounce


  //ensure tic settings are correct
  stepper.setStepMode(TicStepMode::Microstep8);
  stepper.setMaxSpeed(max_speed);

}

void loop() {
  //start test if computer signals
  if (Serial.available()) {
    char data = Serial.read();
    if (data == '1') {
      runTest();  //start the while loop if the Arduino receives '1' from Python code
    }
  }


  
  //check for mode change
  int exit_mode = 0;
  if(digitalRead(A1)){
    exit_mode = 1;
    unsigned long start_exit_time = millis();
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A1)){
      if(millis()-start_exit_time > 1000){
        lcd.clear();
        exit_mode = 2;
        mode_cursor = 0;
      }
    }
    delayWhileResettingCommandTimeout(50);
    lcd.clear();
  }

  switch(mode){
  
    case 00:
      //main menu
      mode = main_menu(exit_mode);
      break;

    case 1:
      //repeat last test
      mode = start_test_menu(exit_mode);
      break;

    case 2:
      //L12 Quickstart
      model = 5;
      mode = start_test_menu(exit_mode);
      break;
    
    case 3:
      //P16 Quickstart
      model = 2;
      mode = start_test_menu(exit_mode);
      break;

    case 4:
      //L16 Quickstart
      model = 6;
      mode = start_test_menu(exit_mode);
      break;

    case 9:
      //quickstart menu
      mode = quickstart(exit_mode);
      break;

    case 10:
      //hardware menu
      mode = hardware_menu(exit_mode);
      break;

    case 11:
      //test menu
      mode = test_menu(exit_mode);
      break;

    case 12:
      //prepare menu
      mode = prepare_menu(exit_mode);
      break;

    case 20:
      //move stepper
      mode = stepper_menu(exit_mode);
      break;

    case 21:
      //move DC
      mode = actuator_menu(exit_mode);
      break;

    case 22:
      //calibrate load cell
      mode = calibration_menu(exit_mode);
      break;

    case 23:
      //re zero load cell
      mode = re_zero_menu(exit_mode);
      break;

    case 30:
      //select model
      mode = model_menu(exit_mode);
      break;

    case 31:
      //mode selection
      mode = mode_menu(exit_mode);
      break;

    case 32:
      //Slow mode
      mode = slow_menu(exit_mode);
      break;

    case 33:
      //Max Force
      mode = max_force_menu(exit_mode);
      break;

    case 34:
      //compressive test
      mode = compression_menu(exit_mode);
      break;

    case 35:
      //early read
      mode = early_read_menu(exit_mode);
      break;

    case 40:
      //cycle actuator
      mode = cycle_actuator_menu(exit_mode);
      break;
 
    case 41:
      //start test manually
      mode = start_test_menu(exit_mode);
      break;

    default: //usually reached when mode is set to -1 at the end of a test
      if(exit_mode || digitalRead(A0)||digitalRead(A2)){
        mode = 00;
      }
      break;
  }


  delayWhileResettingCommandTimeout(1);

}


int move(long int speed){
    stepper.setTargetVelocity(speed);


    speed = (is_forward_left)? speed*-1 : speed; //if inverted, flip the arrows
    lcd.setCursor(15,0);
    if(speed > 0){
      lcd.write((uint8_t)2);//write right
    } else if(speed == 0){
      lcd.write((uint8_t)1);//write stop
    } else if(speed < 0){
      lcd.write((uint8_t)0);//write left
    }


    return 0;
}

int cycle(int size, int pos, int step){
  return (pos+step+size)%size;
}

void runTest(){
//prepare Stepper
if(slow_cursor == 0){
  if(is_tic_microstep_max_32){
    stepper.setMaxSpeed(max_speed/2);
    stepper.setStepMode(TicStepMode::Microstep32);
  } else{
    stepper.setStepMode(TicStepMode::Microstep64);
  }

} else if(slow_cursor == 1){
  if(is_tic_microstep_max_32){
    stepper.setMaxSpeed(max_speed/4);
    stepper.setStepMode(TicStepMode::Microstep32);
  } else{
    stepper.setStepMode(TicStepMode::Microstep128); 
  }
 
} else if(slow_cursor == 2){
  if(is_tic_microstep_max_32){
    stepper.setMaxSpeed(max_speed/8);
    stepper.setStepMode(TicStepMode::Microstep32);
  } else{
    stepper.setStepMode(TicStepMode::Microstep256);
  }

} else{
  if(is_tic_microstep_max_32){
    stepper.setMaxSpeed(max_speed/2);
    stepper.setStepMode(TicStepMode::Microstep32);
  } else{
    stepper.setStepMode(TicStepMode::Microstep64);
  }

}
stepper.energize();
stepper.haltAndSetPosition(compressive_distance*pow(2,slow_cursor)); //0 when doing standard pull test, otherwise sets position to allow for moving in, while still having a max distance



//initialize local variables
int delay_timer = 0;
double local_max = 0;
int peak_count = 0;
int local_peak_count = 0;
double max = 0;
bool buttonClicked=false;
bool skip_forward = false;
const unsigned long start_time = millis();
unsigned long paused_time = 0;
bool endLoop = false;
encoder0Pos = 0;

if(fast_mode){
  lcd.clear();
  while (true)
  {
    int exit = 0;
    if(digitalRead(A1) && model != 0){
      delayWhileResettingCommandTimeout(50);
      while(digitalRead(A1));
      delayWhileResettingCommandTimeout(50);
      model_menu(1);
      break;
    }

    model_menu(0);
    delayWhileResettingCommandTimeout(1);
  }
}

// Set new max force when the user specifies
int max_force_allowed = 300;
if (max_force_override != 0){
  max_force_allowed = max_force_override;
} else if(model == 1){ // P8
  max_force_allowed = 300;
} else if (model == 3){ // PQ12
  max_force_allowed = 100;
} else if (model == 5){ // L12
  max_force_allowed = 300;
} else if(model == 6){
  max_force_allowed = 300;
}

//tell Serial the current test and model
Serial.print("0,0,"); // Zeroes in the first three columns for excel 
Serial.print(encoder0Pos);
Serial.print(",");
Serial.print(models[model]); // Model in the fourth column
Serial.print(",");
if(fast_mode){ // Test type in fifth column
  Serial.print("fast ");
}
if(compressive_distance != 0){
  Serial.print("compressive ");
}
Serial.println("backdrive");

//prepare LCD
lcd.clear();
lcd.setCursor(0,0);
lcd.print("Reading: ");

while (!endLoop) { //loop to collect data after the button has been clicked and while the end condition is unmet

  //counters
  delay_timer = (delay_timer>0)? delay_timer - 200 : 0;
  //end counters
  
  //Track encoder pin A previous state
  //encoder0PinALast = a;

  /*
  control movement
  */
  if(peak_count == 0 && reading < 5){
    if(compressive_distance){
      move(-test_speed*4);
    } else{
      move(test_speed*4);
    }

  } else if(delay_timer<=0 && !skip_forward){ //if not delayed, or position controlled
    if(reading > 200){
      if(compressive_distance){
        move((is_low_resolution)?-test_speed/8:-test_speed/2);
      } else{
        move((is_low_resolution)?test_speed/8:test_speed/2);
      }  
    } else{
      if(compressive_distance){
        move((is_low_resolution)?-test_speed/4:-test_speed);
      } else{
        move((is_low_resolution)?test_speed/4:test_speed);
      }
    }
  } else if(!skip_forward){ //if delayed but not position controlled
    move(0);
  }

  //end movement

  /*
  read measurements
  */
  
  if (scale.is_ready()) {
    reading = scale.get_units(1)*-1;
    if(is_low_resolution) reading = reading-fmod(reading,0.1);
    if(compressive_distance) reading *= -1;
    
    //if reading high, lower max speed
    if (reading > 220){
      stepper.setMaxSpeed(test_speed);
    } else if(reading < 200){
      stepper.setMaxSpeed(max_speed);
    }
    

    //display force reading
    lcd.setCursor(0, 1);
    lcd.print(reading);
    lcd.setCursor(5,1);
    lcd.write("N");
    

    //only save reading if moving, pulling and not skipping forward
    if(delay_timer<=0 && !skip_forward && reading>0 && (peak_count>4||fast_mode||early_reading_mode)){
    Serial.print((millis()-start_time-paused_time)/1000.0);
    Serial.print(",");
    Serial.print(reading);
    Serial.print(",");
    Serial.println(encoder0Pos*0.01);
    }
    
    
    //checking for force drop
    if(    delay_timer   &&    reading < local_max-0.03  ){ //if still pulling reset delay timer
      delay_timer = stop_time;
      local_max = reading;
      if(peak_count>5 || fast_mode || early_reading_mode ){
        Serial.print((millis()-start_time-paused_time)/1000.0);
        Serial.print(",");
        Serial.print(reading);
        Serial.print(",");
        Serial.println(encoder0Pos*0.01);
      }

    }else if( (reading > local_max   ||    local_max ==  0)     &&    !delay_timer){ //set local max if new reading is greater
      local_max = reading;
    }else if(reading < (local_max*.9) && local_max > 5 && reading < local_max-2 && !delay_timer){ //start delay time if slipped
      //check if new max peak
      if(local_max>max){
        max = local_max;
      }
      peak_count++;
      local_peak_count++;
      local_max = 0;
      
      //if using fastmode and enough peaks have occurred, skip forward
      if(local_peak_count == peaks_per_skip && fast_mode){
        int is_compress_dir = (compressive_distance)? -1:1;
        stepper.setTargetPosition(stepper.getCurrentPosition()+skip_distance*is_compress_dir);
        local_peak_count = 0;
        skip_forward = true;
      } else{
        delay_timer = stop_time;
      }
    }
  }

//when position control reaches its end, reset the skip trigger
if(skip_forward){
    if(stepper.getCurrentPosition()==stepper.getTargetPosition()){
      skip_forward = false;
    }
}


  //end read measurements
  /*
  run time clock
  */
  lcd.setCursor(9,1);
  int Seconds = (millis()-start_time-paused_time)/1000;
  int minutes = Seconds/60;
  Seconds = Seconds%60;
  if(minutes < 100){
    lcd.print("0");
  }
  if(minutes < 10){
    lcd.print("0");
  }
  lcd.print(minutes);
  lcd.print(":");
  if(Seconds<10){
    lcd.print("0");
  }
  lcd.print(Seconds);

  /*
  check exit conditions
  */

 //stop if recieve 0 from serial
  if (Serial.available()) {
    char data = Serial.read();
    if (data == '0') {
      lcd.setCursor(0,0);
      lcd.write("halted  ");
      Serial.println("0,0,0,0,0,100"); // 100 means serial exit condition
      break;
    }
  }

//stop if max reading is very large
  if(reading > max_force_allowed){
    //print blank into Serial
    Serial.print((millis()-start_time-paused_time)/1000.0);
    Serial.print(",");
    Serial.println(0);

    stepper.haltAndHold();
    delayWhileResettingCommandTimeout(50);
    while(!scale.is_ready());
    reading = scale.get_units(1)*-1;
    if(compressive_distance) reading *= -1;
    delayWhileResettingCommandTimeout(50);
    digitalWrite(11,LOW);
    digitalWrite(12,HIGH);
    lcd.setCursor(15,0);
    lcd.write((uint8_t)2);
    delayWhileResettingCommandTimeout(200);
    while(!scale.is_ready());
    double second_reading = scale.get_units(1)*-1;
    if(compressive_distance) second_reading *= -1;

    if(second_reading < reading ){
      bool end_while = false;
      while(!end_while){
        delayWhileResettingCommandTimeout(50);
        reading = second_reading;
        if(scale.is_ready()){
          second_reading = scale.get_units(1)*-1;
          if(compressive_distance) second_reading *= -1;
        }

        if (second_reading+0.01 >= reading){
          end_while = true;
        }
        if(reading > max_force_allowed+5){
          end_while = true;
        }
        if(reading < 200){
          end_while = true;
        }
      }
      digitalWrite(11,LOW);
      digitalWrite(12,LOW);
    }else {
      lcd.setCursor(0,0);
      lcd.write("max force");
      digitalWrite(11,LOW);
      digitalWrite(12,LOW);
      Serial.println("0,0,0,0,0,200"); // 200 means max reading exit condition
      break;
    }
  }

  //stop if at end of actuator
  if(peak_count>10 && reading > (max+50) && reading > (max*1.8)){
    stepper.haltAndHold();
    lcd.setCursor(0,0);
    lcd.write("actuator end?");

    Serial.println("0,0,0,0,0,300"); // 300 means actuator end exit condition

    lcd.setCursor(15,0);
    lcd.write((uint8_t)1);
    while (true)
    {
      //pause for confirmation
      if(digitalRead(A0)||digitalRead(A1)){ //end if centre or left button
        lcd.setCursor(0,0);
        lcd.write("actuator end ");
        endLoop = true;
        delayWhileResettingCommandTimeout(20);
        while(digitalRead(A0)||digitalRead(A1));
        delayWhileResettingCommandTimeout(20);
        break;
      } else if(digitalRead(A2)){ //continue if right button
        lcd.clear();
        delayWhileResettingCommandTimeout(20);
        while(digitalRead(A2));
        delayWhileResettingCommandTimeout(20);
        lcd.setCursor(0,0);
        lcd.write("reading:");
        break;
      } else if(digitalRead(A3)){ //quick end if limit switch hit
        break;
      }

      delayWhileResettingCommandTimeout(1);
    }
  }

  //stop if hit end stop
  if(digitalRead(A3)){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.write("end stop hit");
    Serial.println("0,0,0,0,0,400"); // 400 means limit switch exit condition
    break;
  }

  //stop if hit end stop
  if((compressive_distance && stepper.getCurrentPosition() <= 1) || (early_reading_mode == 1 && peak_count > 4)){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.write("max distance");
    Serial.println("0,0,0,0,0,500"); // 500 means max distance exit condition
    break;
  }

  //stop on button press
  if(digitalRead(A0)||digitalRead(A1)||digitalRead(A2)){
    lcd.setCursor(0,0);
    lcd.write("stop button");
    buttonClicked = true;
    break;
  }

  //small delay
  delayWhileResettingCommandTimeoutandObservingLimits(200);
  } // End of while loop

  /*
  Program Ended
  */
stepper.haltAndHold();
stepper.setMaxSpeed(max_speed);
delayWhileResettingCommandTimeout(100);
//extend slightly unless user manually stopped
if(!buttonClicked){
  lcd.setCursor(15,0);
  lcd.write((uint8_t)0);

  //rewind
  if(compressive_distance){
    move(stepper.getMaxSpeed());
  } else{
    move(-stepper.getMaxSpeed());
  }
  //rewind until lower force
  while(1){
    if(scale.is_ready()){
      reading = scale.get_units(1)*-1;
      if(compressive_distance) reading *= -1;
      if(reading < 100){
        break;
      }
    }
    delayWhileResettingCommandTimeoutandObservingLimits(10);
  }
  //move slightly further to give slack
  delayWhileResettingCommandTimeoutandObservingLimits(stop_time/2);
}

//stop and disable stepper
move(0);
stepper.deenergize();
stepper.setMaxSpeed(max_speed);
stepper.setStepMode(TicStepMode::Microstep8);
lcd.setCursor(15,0);
lcd.write((uint8_t)3);
mode = -1;
}  // End of test loop


/*
Menus
*/
int generic_menu(int current, int previous, int exit, int menu_selections, int menu_directions[], String menu_options[]){
  
  //update selection
  if(digitalRead(A0)){
    mode_cursor = cycle(menu_selections,mode_cursor,-1);
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A0));
    delayWhileResettingCommandTimeout(50);
  } else if(digitalRead(A2)){
    mode_cursor = cycle(menu_selections,mode_cursor,1);
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A2));
    delayWhileResettingCommandTimeout(50);
  }


  //display
  lcd.setCursor(0,1);
  lcd.print("<");
  lcd.print(menu_options[mode_cursor]);
  lcd.setCursor(15,1);
  lcd.print(">");


  //exit
  if(exit == 1){
    lcd.clear();
    int out =  menu_directions[mode_cursor];
    mode_cursor = 0;
    return out;
  } else if(exit==2){
    lcd.clear();
    mode_cursor = 0;
    return previous;
  } else {
    return current;
  }
}

int main_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("Main Menu");
  int menu_selections = 4;
  int menu_directions[] = {
    9,10,11,12
  };

  //all names should be 14 characters to prevent rewriting if a longer name appears
  String menu_options[] = {
    "quickstart    ",
    "hardware      ", 
    "test          ",
    "prepare       "

  };
  
  return generic_menu(00,00,exit,menu_selections,menu_directions,menu_options);
}

int quickstart(int exit) {
  lcd.setCursor(0,0);
  lcd.print("Quickstart  ");
  int menu_selections = 5;
  int menu_directions[] = {
    1,2,3,4,00
  };

  //all names should be 14 characters to prevent rewriting if a longer name appears
  String menu_options[] = {
    "last test     ",
    "L12           ",
    "P16           ",
    "L16           ",
    "back          "
  };
  
  return generic_menu(9,00,exit,menu_selections,menu_directions,menu_options);
}

int hardware_menu(int exit){
  if(!disable_actuator_control){

    lcd.setCursor(0,0);
    lcd.print("Hardware Menu");
    int menu_selections = 5;
    int menu_directions[] = {
      20,21,22,23,00
    };
  //all names should be 14 characters to prevent issues when a longer name appears
    String menu_options[] = {
      "move stepper  ",
      "adj actuator  ",
      "calibrate load",
      "re-zero load  ",
      "back          "
    };

    return generic_menu(10,00,exit,menu_selections,menu_directions,menu_options);
  
  } else{

    lcd.setCursor(0,0);
    lcd.print("Hardware Menu");
    int menu_selections = 4;
    int menu_directions[] = {
      20,22,23,00
    };
  //all names should be 14 characters to prevent issues when a longer name appears
    String menu_options[] = {
     "move stepper  ",
      "calibrate load",
      "re-zero load  ",
     "back          "
    };
  
    return generic_menu(10,00,exit,menu_selections,menu_directions,menu_options);
  
  }
}

int stepper_menu(int exit){
  lcd.setCursor(0,0);
  lcd.write("move stepper");
  stepper.energize();
  if(((digitalRead(A0)&&!is_forward_left) || (digitalRead(A2)&&is_forward_left))&&(!is_two_limit_switch || !digitalRead(A6))){//inwards
    stepper.setTargetVelocity(-stepper.getMaxSpeed());
    lcd.setCursor(15,0);
    if(is_forward_left){
      lcd.write((uint8_t)2);
    } else{
      lcd.write((uint8_t)0);  
    }
    
  } else if(((digitalRead(A0)&&is_forward_left) || (digitalRead(A2)&&!is_forward_left))&&!digitalRead(A3)){//outwards
    stepper.setTargetVelocity(stepper.getMaxSpeed());
    lcd.setCursor(15,0);
    if(is_forward_left){
      lcd.write((uint8_t)0);
    } else{
      lcd.write((uint8_t)2);
    }
  } else if(digitalRead(A3)){//end stop
    stepper.haltAndHold();
    lcd.setCursor(15,0);
    lcd.write((uint8_t)3);
  }else if(digitalRead(A6) && is_two_limit_switch){//end stop
    stepper.haltAndHold();
    lcd.setCursor(15,0);
    lcd.write((uint8_t)3);
  }else {//no input
    stepper.setTargetVelocity(0);
    lcd.setCursor(15,0);
    lcd.write((uint8_t)1);
  }

  if(exit){
    lcd.clear();
    //stop and depower stepper
    stepper.setTargetVelocity(0);
    stepper.deenergize();
    return 10; //10 -> hardware menu
  } else{
    return 20; //20 -> stepper control
  }
}

int actuator_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("adj actuator");
  if(digitalRead(A0)){//left
    digitalWrite(11,HIGH);
    digitalWrite(12,LOW);
    lcd.setCursor(15,0);
    lcd.write((uint8_t)0);
  } else if(digitalRead(A2)){//right
    digitalWrite(11,LOW);
    digitalWrite(12,HIGH);
    lcd.setCursor(15,0);
    lcd.write((uint8_t)2);
  } else{//stop
    digitalWrite(11,LOW);
    digitalWrite(12,LOW);
    lcd.setCursor(15,0);
    lcd.write((uint8_t)1);
  }

  if(exit){
    lcd.clear();
    //stop actuator
    digitalWrite(11,LOW);
    digitalWrite(12,LOW);
    return 10; //10 -> hardware menu
  } else{
    return 21; //21 -> actuator control
  }
}

int calibration_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("Calibrate Force");
  
  lcd.setCursor(0,1);
  lcd.print(scale.get_units()*-1/9.81,3);//print equivalent mass
  lcd.setCursor(5,1);
  lcd.print("kg");

  lcd.setCursor(10,1);
  lcd.print(scale.get_units()*-1,3);
  lcd.setCursor(15,1);
  lcd.print("N");

  if(digitalRead(A0)){
    force_calibration_constant += 10; //lower apparent mass
    delayWhileResettingCommandTimeout(1);
    scale.set_scale(force_calibration_constant);
  }
  if(digitalRead(A2)){
    force_calibration_constant -= 10; //raise apparent mass
    delayWhileResettingCommandTimeout(1);
    scale.set_scale(force_calibration_constant);
  }

  if(exit){
    lcd.clear();
    //save force calibration mode
    EEPROM.put(0,force_calibration_constant);
    return 10; //10 -> hardware menu
  } else{
    return 22; //22 -> calibration control
  }
}

int re_zero_menu(int exit){
  //wait to zero force input
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("zero force?");
  stepper.energize();
  while (!digitalRead(A1))
  {
    if(digitalRead(A0)){
      move(-stepper.getMaxSpeed());
      lcd.setCursor(15,0);
      lcd.write((uint8_t)0);
    } else if(digitalRead(A2)){
      move(stepper.getMaxSpeed());
      lcd.setCursor(15,0);
      lcd.write((uint8_t)2);
    } else{
      move(0);
      lcd.setCursor(15,0);
      lcd.write((uint8_t)1);
    }
    delayWhileResettingCommandTimeout(1);
  }

  //reset for waiting
  scale.tare();//set 0 on loadcell
  lcd.clear();
  lcd.setCursor(5,0);
  lcd.print("done");
  delayWhileResettingCommandTimeout(1000);
  //note - this function holds the arduino captive as it should never be interupted
  //when finished return to hardware menu
  return 10;
}


int test_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("Test Menu");
  int menu_selections = 6;
  int menu_directions[] = {
    30,/*31,*/32,33,34,35,00
  };
  //all names should be 14 characters to ensure proper refresh
  String menu_options[] = {
    "set test model",
    //"set fast test ", // fastmode has been disabled due to lack of use case (reads ~5N higher on average, making unsuitable for most tests)
    "set slow test ",
    "set max force ",
    "set compress  ",
    "set early read",
    "back          "
  };
  return generic_menu(11,00,exit,menu_selections,menu_directions,menu_options);
}

int model_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("model");
  lcd.setCursor(1,1);
  lcd.print("<");
  lcd.setCursor(14,1);
  lcd.print(">");
  if(digitalRead(A0)){
    model = (model + model_count - 1)%model_count;
    lcd.setCursor(3,1);
    lcd.print(models[model]);
    delayWhileResettingCommandTimeout(20);
    while (digitalRead(A0));
    delayWhileResettingCommandTimeout(20);
  } else if(digitalRead(A2)){
    model = (model + model_count + 1)%model_count;
    lcd.setCursor(3,1);
    lcd.print(models[model]);
    delayWhileResettingCommandTimeout(20);
    while (digitalRead(A2));
    delayWhileResettingCommandTimeout(20);
  } else{
    lcd.setCursor(3,1);
    lcd.print(models[model]);
  }

  if(exit){
    lcd.clear();
    return 11; //11 -> test menu
  } else{
    return 30; //30 -> model menu
  }
}

int mode_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("fast mode:");
  int option_count = 5;
  int step_options[] = {
    0,5,10,20,30
  };
  fast_mode = (step_options[fast_mode_cursor] == 0)? false:true;
  skip_distance = 3200*((int32_t)step_options[fast_mode_cursor]);
  if(digitalRead(A0)){
    if(fast_mode_cursor>0){
      fast_mode_cursor--;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A0));
    delayWhileResettingCommandTimeout(50);
  } else if(digitalRead(A2)){
    if(fast_mode_cursor<option_count-1){
      fast_mode_cursor++;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A2));
    delayWhileResettingCommandTimeout(50);
  }

  lcd.setCursor(0,1);
  if(fast_mode_cursor == 0){
    lcd.print("   off        > ");
  } else {
    lcd.print(" < ");
    lcd.print(step_options[fast_mode_cursor]);
    lcd.print("mm   ");
    lcd.setCursor(14,1);
    if(fast_mode_cursor+1 < option_count){
      lcd.print(">");
    } else{
      lcd.print(" ");
    }
  }

  if(exit){
    lcd.clear();
    return 11; //11 -> test menu
  } else{
    return 31; //31 -> mode menu
  }
}

int slow_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("slow test:");
  int option_count = 3;
  int step_options[] = {
    1,2,4
  };
  
  skip_distance = 3200*((int32_t)step_options[slow_cursor]);
  if(digitalRead(A0)){
    if(slow_cursor>0){
      slow_cursor--;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A0));
    delayWhileResettingCommandTimeout(50);
  } else if(digitalRead(A2)){
    if(slow_cursor<option_count-1){
      slow_cursor++;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A2));
    delayWhileResettingCommandTimeout(50);
  }

  lcd.setCursor(0,1);
  if(slow_cursor == 0){
    lcd.print("   off        > ");
  } else {
    lcd.print(" < ");
    lcd.print("x1/");
    lcd.print(step_options[slow_cursor]);
    lcd.print(" speed");

    lcd.setCursor(14,1);
    if(slow_cursor+1 < option_count){
      lcd.print(">");
    } else{
      lcd.print(" ");
    }
  }

  if(exit){
    lcd.clear();
    return 11; //11 -> test menu
  } else{
    return 32; //31 -> slow menu
  }
}

int max_force_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("set max force");
  int option_count = 20;
  /*Note: force options are limited by the rig max force*/
  int force_options[] = {
    0,50,100,150,200,250,300,350,400,500,600,700,800,900,1000,1100,1200,1300,1400,1500
  };

  max_force_override = force_options[mode_cursor];

  if(digitalRead(A0)){
    if(mode_cursor>0){
      mode_cursor--;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A0));
    delayWhileResettingCommandTimeout(50);
  } else if(digitalRead(A2)){
    if(mode_cursor<option_count-1){
      if(force_options[mode_cursor+1] <= rig_max_force)
      mode_cursor++;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A2));
    delayWhileResettingCommandTimeout(50);
  }

  lcd.setCursor(0,1);
  if(mode_cursor == 0){
    lcd.print("   default    > ");
  } else {
    lcd.print(" < ");
    lcd.print(force_options[mode_cursor]);
    lcd.print("N    ");
    lcd.setCursor(14,1);
    if(mode_cursor+1 < option_count){
      //ensure the next option is allowed on the rig
      if(force_options[mode_cursor+1] <= rig_max_force){
        lcd.print(">");
      } else{
        lcd.print(" ");
      }


    } else{
      lcd.print(" ");
    }
  }

  if(exit){
    lcd.clear();
    mode_cursor = 0;
    return 11; //11 -> test menu
  } else{
    return 33; //33 -> max force menu
  }
}

int compression_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("compress dist:");
  int option_count = 11;
  int step_options[] = {
    0,5,10,15,20,25,30,40,50,70,100
  };
  compressive_distance = 3200*((int32_t)step_options[compressive_cursor]);
  if(digitalRead(A0)){
    if(compressive_cursor>0){
      compressive_cursor--;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A0));
    delayWhileResettingCommandTimeout(50);
  } else if(digitalRead(A2)){
    if(compressive_cursor<option_count-1){
      compressive_cursor++;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A2));
    delayWhileResettingCommandTimeout(50);
  }

  lcd.setCursor(0,1);
  if(compressive_cursor == 0){
    lcd.print("   off        > ");
  } else {
    lcd.print(" < ");
    lcd.print(step_options[compressive_cursor]);
    lcd.print("mm   ");
    lcd.setCursor(14,1);
    if(compressive_cursor+1 < option_count){
      lcd.print(">");
    } else{
      lcd.print(" ");
    }
  }

  if(exit){
    lcd.clear();
    return 11; //11 -> test menu
  } else{
    return 34; //34 -> compressive menu
  }
}

int early_read_menu(int exit){
   lcd.setCursor(0,0);
  lcd.print("early readings:");
  int option_count = 3;
  String mode_options[] = {
    "off","only","on"
  };
 
  if(digitalRead(A0)){
    if(early_reading_mode>0){
      early_reading_mode--;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A0));
    delayWhileResettingCommandTimeout(50);
  } else if(digitalRead(A2)){
    if(early_reading_mode<option_count-1){
      early_reading_mode++;
    }
    delayWhileResettingCommandTimeout(50);
    while(digitalRead(A2));
    delayWhileResettingCommandTimeout(50);
  }

  lcd.setCursor(0,1);
  if(early_reading_mode == 0){
    lcd.print("   off        > ");
  } else {
    lcd.print(" < ");
    lcd.print(mode_options[early_reading_mode]);
    lcd.print("     "); //in case of word length mismatch
    lcd.setCursor(14,1);
    if(early_reading_mode+1 < option_count){
      lcd.print(">");
    } else{
      lcd.print(" ");
    }
  }

  if(exit){
    lcd.clear();
    return 11; //11 -> test menu
  } else{
    return 35; //34 -> early read menu
  }
}

int prepare_menu(int exit){
  lcd.setCursor(0,0);
  lcd.print("Prepare Menu");
  int menu_selections = 3;
  int menu_directions[] = {
    40,41,00
  };
  //all names should be 14 characters to ensure proper screen refresh
  String menu_options[] = {
    "cycle actuator",
    "start test    ",
    "back          "
  };
  return generic_menu(12,00,exit,menu_selections,menu_directions,menu_options);
}

int cycle_actuator_menu(int exit){
  int time_option_count = 11;
  int time_step_options[] = {
    0,4,5,6,7,8,9,10,15,20,25
  };

  int count_option_count = 8;
  int count_options[] = {
    0,5,10,20,30,50,70,100
  };

  //menu 1. can escape before making a selection
  if(digitalRead(A0) && mode_cursor>0){

      mode_cursor--;
      delayWhileResettingCommandTimeout(50);
      while (digitalRead(A0));
      delayWhileResettingCommandTimeout(50);
  }else if(digitalRead(A2) && mode_cursor<count_option_count - 1){
      mode_cursor++;
      delayWhileResettingCommandTimeout(50);
      while (digitalRead(A2));
      delayWhileResettingCommandTimeout(50);
  }


  lcd.setCursor(0,0);
  lcd.print("cycle count");

  //print cycle count
  lcd.setCursor(0,1);
  if(mode_cursor==0){
    lcd.print("   ");
    lcd.print("cancel  ");
  } else{
  lcd.print(" < ");
  lcd.print(count_options[mode_cursor]);
  lcd.print("      ");
  }

  lcd.setCursor(13,1);
  if(mode_cursor < count_option_count){
    lcd.print(" > ");
  } else{
    lcd.print("   ");
  }

  if(digitalRead(A1)){
      lcd.clear();
      delayWhileResettingCommandTimeout(50);
      while (digitalRead(A1));
      delayWhileResettingCommandTimeout(50);

      int count = count_options[mode_cursor];
      //exit if setting is 0
      if(mode_cursor == 0){
        lcd.clear();
        return 12;
      }

      //set to 1 to not default on cancel
      mode_cursor = 1;

      int drive_time;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("set drive time:");
      //nested menu. can't escape anymore
      while (!digitalRead(A1))
      {
        drive_time = time_step_options[mode_cursor];
        lcd.setCursor(0,1);
        if(mode_cursor==0){
          lcd.print("   ");
          lcd.print("cancel  ");

        } else{
          lcd.print(" < ");
          lcd.print(drive_time);
          lcd.print("      ");
        }

        lcd.setCursor(13,1);
        if(mode_cursor < time_option_count - 1){ //ensure not on the final option
          lcd.print(" > ");
        } else{
          lcd.print("   ");
        }
        

        //listen to buttons
        if(digitalRead(A0) && mode_cursor>0){
          mode_cursor--;
          delayWhileResettingCommandTimeout(50);
          while (digitalRead(A0));
          delayWhileResettingCommandTimeout(50);
        } else if(digitalRead(A2) && mode_cursor<time_option_count - 1){ //ensure not on the final option
          mode_cursor++;
          delayWhileResettingCommandTimeout(50);
          while (digitalRead(A2));
          delayWhileResettingCommandTimeout(50);
        }
      }
      
      delayWhileResettingCommandTimeout(50);
      while(digitalRead(A1));
      delayWhileResettingCommandTimeout(50);

      lcd.clear();


      //return if cancel selected
      if(mode_cursor==0){
        return 12;
      }


      lcd.setCursor(0,0);
      lcd.print("cycles left:");
      for(int i = 0; i < count; i++){
        lcd.setCursor(0,1);
        lcd.print(count-i);
        lcd.print("  ");
        
        //extend for drive time
        digitalWrite(11,LOW);
        digitalWrite(12,HIGH);
        lcd.setCursor(15,0);
        lcd.write((uint8_t)2);

        if(digitalRead(A0) || digitalRead(A1) || digitalRead(A2)) {
          break;
        }
        
        delayWhileResettingCommandTimeout(1000*drive_time);

        //retract for drive time
        digitalWrite(11,HIGH);
        digitalWrite(12,LOW);
        lcd.setCursor(15,0);
        lcd.write((uint8_t)0);

        delayWhileResettingCommandTimeout(1000*drive_time);

        if(digitalRead(A0) || digitalRead(A1) || digitalRead(A2)) {
          break;
        }
      }
      //stop moving
      digitalWrite(11,LOW);
      digitalWrite(12,LOW);
      //reset lcd
      lcd.clear();
      lcd.setCursor(6,0);
      lcd.print("done");

      delayWhileResettingCommandTimeout(50);
      while(!digitalRead(A0)&&!digitalRead(A1)&&!digitalRead(A2));
      delayWhileResettingCommandTimeout(50);
      while(digitalRead(A0) || digitalRead(A1) || digitalRead(A2));
      delayWhileResettingCommandTimeout(50);
      
      mode_cursor = 0;
      lcd.clear();
      return 12;
      

  }

  return 40; //stay on current menu

  //return 12; //return to menu
}

int start_test_menu(int exit){
  //todo: make this an actual menu instead of just force starting a test
  if (exit == 0) runTest();
  return 9;
}


//Interrupt handlers for encoder
void CountA()
{
  a = digitalRead(encoder0PinA); 
  b = digitalRead(encoder0PinB);
  if ((encoder0PinALast == LOW) && (a == HIGH)) { 
    if (b == LOW) { 
      encoder0Pos--; 
    } 
    else { 
      encoder0Pos++; 
    } 
  }
  else{
    if(b == HIGH){
      encoder0Pos--;
    }
    else{
      encoder0Pos++;
    }
  }
  encoder0PinALast = a;
}
/*void StateB()
{
  b = digitalRead(encoder0PinB);
}*/