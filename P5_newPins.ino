// Pins
#define UPIN 12 //button input
#define BIN_3 27 //motor 2A
#define BIN_4 33 //motor 2B
#define BIN_1 25 //motor 1A
#define BIN_2 26 //motor 1B
#define POT 15 
//  encoder.attachHalfQuad(39, 34);  ENCODERS
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 14;
#include <ESP32Encoder.h>
#include "HX711.h"
ESP32Encoder encoder;
HX711 scale;

// Tea Variables (changeable by user)
int brewTime = 90; //seconds
int waterTempTarget = 85; //celsius
int teaMassTarget = 6; //grams
int waterMassTarget = 200; //grams
int cupsOfTea = 1;


// State Machine
byte state = 2; 
//volatile bool userSelectFlag = false;
volatile bool brewedTeaFlag = false;
volatile bool buttonIsPressed = false;
volatile bool DEBOUNCINGflag = false;

// Motor Speed Control
int omegaSpeed = 0;
int omegaDes = 0;
int omegaMax = 20; 
int D = 0;
int dir = 1;
int potReading = 0;
int Kp = 40; 
int Ki = 8;
int IMax = 0;
float error_sum = 0;

// Motor PWM
const int freq = 5000;
const int ledChannel_1 = 1;
const int ledChannel_2 = 2;
const int resolution = 8;
const int MAX_PWM_VOLTAGE = 255;
const int NOM_PWM_VOLTAGE = 150;
volatile int count = 0;                  // encoder count
volatile bool deltaT = false;            // check timer interrupt for encoder count
hw_timer_t * timer0 = NULL; // button debouncer
hw_timer_t* timer1 = NULL; // encoder
hw_timer_t* timer2 = NULL; //tea
portMUX_TYPE timerMux0 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;

// %%% ISRs %%%%
void IRAM_ATTR onTime1() { //encoder speed readoutt
  portENTER_CRITICAL_ISR(&timerMux1);
  count = encoder.getCount();
  encoder.clearCount();
  deltaT = true;
  portEXIT_CRITICAL_ISR(&timerMux1);
}

/*
void IRAM_ATTR user_isr() { //electronic display OR button
  userSelectFlag == true;     
}
*/

void IRAM_ATTR onTime2() {
  brewedTeaFlag = true; // 
}

void IRAM_ATTR button_isr() {  // button debouncer ISR
    buttonIsPressed = true;
    DEBOUNCINGflag = true;  
    timerStart(timer0);     
  }


void IRAM_ATTR onTime0() { // button debouncer flag reset
  portENTER_CRITICAL_ISR(&timerMux0);
  DEBOUNCINGflag = false;
  portEXIT_CRITICAL_ISR(&timerMux0);
  timerStop(timer0);
}

void setup() {
  Serial.begin(115200); 
  ESP32Encoder::useInternalWeakPullResistors = puType::up; //for encoders
  encoder.attachHalfQuad(34, 39); // Motor 1
  encoder.attachHalfQuad(36, 4);  // Motor 2
  encoder.setCount(0); 
  ledcAttach(BIN_1, freq, resolution); //motors
  ledcAttach(BIN_2, freq, resolution);
  ledcAttach(BIN_3, freq, resolution);
  ledcAttach(BIN_4, freq, resolution);
  pinMode(POT, INPUT);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  timer1 = timerBegin(1000000);            // Encoder readout timer: Set timer frequency to 1Mhz
  timerAttachInterrupt(timer1, &onTime1);  // Attach onTimer1 function to our timer.
  timerAlarm(timer1, 10000, true, 0);      // 10000 * 1 us = 10 ms, autoreload true

  pinMode(UPIN, INPUT); 
  timer0 = timerBegin(1000000);            // button debouncing timer
  timerAttachInterrupt(timer0, &onTime0); 
  timerAlarm(timer0, 1000000, true, 0); 
  attachInterrupt(UPIN, button_isr, RISING);

  timer2 = timerBegin(1000000);            // button debouncing timer
  timerAttachInterrupt(timer2, &onTime2); 
  timerAlarm(timer2, 10000000, true, 0); 
}

// %%%%%%%%%%%%%%%%%%%%%%%% MAIN LOOP %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void loop() {
  switch (state) {

    case 1: // IDLE
      Serial.println("state 1: idling");
      
      if (CheckForButtonPress() == 0) { //tea 1
        buttonIsPressed = false;
        Serial.println("button press tea 1");
        // startAuger();
        startAuger1();
        Serial.println("started auger");
        state = 2;
      }
      else if (CheckForButtonPress() == 1) { //tea 2
        buttonIsPressed = false;
        Serial.println("button press tea 2");
        // startAuger();
        startAuger2();
        Serial.println("started auger");
        state = 2;
      }
      break;

    case 2: // DISPENSING LEAVES
      Serial.println("state 2: dispensing leaves");
      startAuger2();
      /*
      if (teaMassChecker()) {
        stopAuger();
        startHeater();
        // state = 3;
        state = 4;
      }
      */
      break;

    case 3: // HEATING WATER (skip for P5)
      Serial.println("state 3: heating water");/*
      if (waterTempChecker()) {
        stopHeater();
        startWater();
        state = 4;
      }*/

      break;

    case 4: // DISPENSING WATER
      Serial.println("state 4: dispensing water");
      if (waterMassChecker()) {
        stopWater();
        //brewTimerTea();
        state = 5;
      }
      break;

    case 5: // BREWING TEA
      Serial.println("state 5: brewing tea");
      delay(10000);
      /*if (brewedTeaFlag) {
        brewedTeaFlag = false;
        startTea();
        state = 6;
      }*/
      state = 6;
      break;

    case 6: // DISPENSING TEA
      Serial.println("state 6: dispensing finished tea");
      if (loadCellConstant()) {
        delay(10000);
        stopTea;
        state = 1;
      }
      break;

    default: //ERROR
      Serial.println("SM ERROR");
      break;
  }

}

// %%%%%%%%%%%%%%%%%%%%%%%% END MAIN LOOP %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%



// %%%%%%%%%%%%%%%%%%%%%%%% FUNCTIONS %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %% START AND STOP 
void startAuger1() {
  // starts motor, fixed speed

  if (deltaT) { //encoder count flag (10ms)
    portENTER_CRITICAL(&timerMux1);
    deltaT = false;
    portEXIT_CRITICAL(&timerMux1);

    omegaSpeed = count;
    potReading = analogRead(POT);
    omegaDes = -dir * map(potReading, 0, 4095, -omegaMax, omegaMax);  // map speed to pot

    error_sum = error_sum + (omegaDes-omegaSpeed)/10;
    D = Kp*(omegaDes-omegaSpeed)+Ki*(error_sum);  // P/PI controller 
    
    if (D > MAX_PWM_VOLTAGE) { //speed safeguards
      D = MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    } else if (D < -MAX_PWM_VOLTAGE) {
      D = -MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    }

    D=150;

    if (D > 0) { //motor inputs by PWM
      ledcWrite(BIN_1, LOW);
      ledcWrite(BIN_2, D);
    } else if (D < 0) {
      ledcWrite(BIN_2, LOW);
      ledcWrite(BIN_1, -D);
    } else {
      ledcWrite(BIN_2, LOW);
      ledcWrite(BIN_1, LOW);
    }
    plotControlData();
  }
}

void startAuger2() {
  // starts motor, fixed speed

  if (deltaT) { //encoder count flag (10ms)
    portENTER_CRITICAL(&timerMux1);
    deltaT = false;
    portEXIT_CRITICAL(&timerMux1);

    omegaSpeed = count;
    potReading = analogRead(POT);
    omegaDes = -dir * map(potReading, 0, 4095, -omegaMax, omegaMax);  // map speed to pot

    error_sum = error_sum + (omegaDes-omegaSpeed)/10;
    D = Kp*(omegaDes-omegaSpeed)+Ki*(error_sum);  // P/PI controller 
    
    if (D > MAX_PWM_VOLTAGE) { //speed safeguards
      D = MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    } else if (D < -MAX_PWM_VOLTAGE) {
      D = -MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    }

    D=150;

    if (D > 0) { //motor inputs by PWM
      ledcWrite(BIN_3, LOW);
      ledcWrite(BIN_4, D);
    } else if (D < 0) {
      ledcWrite(BIN_4, LOW);
      ledcWrite(BIN_3, -D);
    } else {
      ledcWrite(BIN_3, LOW);
      ledcWrite(BIN_4, LOW);
    }
    plotControlData();
  }
}

void plotControlData() {
  Serial.print("Speed:");
  Serial.print(omegaSpeed);
  Serial.print(" ");
  Serial.print("Desired_Speed:");
  Serial.print(omegaDes);
  Serial.print(" ");
  Serial.print("PWM_Duty/10:");
  Serial.println(D / 10);  //PWM is scaled by 1/10 to get more intelligible graph
}

void stopAuger() {
  // stop motor (PWM all 1)
    ledcWrite(BIN_2, LOW);
    ledcWrite(BIN_1, LOW);
    ledcWrite(BIN_3, LOW);
    ledcWrite(BIN_4, LOW);
}

void startHeater() {
  // turns on heater, skip for P5
}

void stopHeater() {
  // turns off heater, skip for P5
}

void startWater() {
  // dispenses water, manual for P5
}

void stopWater() {
  // stops dispensing water, manual for P5
}

void startTea() {
  // dispenses finished tea, manual for P5
}

void stopTea() {
  // closes tea valve, manual for P5
}

void brewTimerTea()  {
  // starts timer for brewing tea, when timer is done set brewedTeaFlag=True 
  /*TeaTimer = timerBegin(1);
  timerAttachInterrupt(TeaTimer, &onTeaTimer);
  timerAlarm(TeaTimer, 10, false, 0);*/
  timerStart(timer2);
  //timerAttachInterrupt(timer2, &onTime2);
}


// %%% MASS AND TEMP CHECKERS

bool waterTempChecker() {
  // measures if water temperature has hit target, skip for P5
}

bool waterMassChecker() {
  // measures if correct mass of water has been dispensed
  if (checkScale() > 400000) {
    return true;
  }
  else {
    return false;
  }
}

bool teaMassChecker() {
  // measures if correct mass of tea leaves has been dispensed
    if (checkScale() > 200000) {
    return true;
  }
  else {
    return false;
  }
}

bool loadCellConstant() {
  // checks if all tea has been drained out
    if (checkScale() < 200000) {
    return true;
  }
  else {
    return false;
  }
}

signed int checkScale() { //outputs load cell reading 
    if (scale.is_ready()) {
    scale.set_scale();
    long reading = scale.get_units(10);
    Serial.println("Weight:");
    Serial.println(reading);
    return reading;
  } 
}

bool CheckForButtonPress() { //debounced button
  if (buttonIsPressed == true && DEBOUNCINGflag == false) {
    portENTER_CRITICAL_ISR(&timerMux0);
    DEBOUNCINGflag = true;
    portEXIT_CRITICAL_ISR(&timerMux0);
    timerStart(timer0);
    return true;
  }
  return false;  
}

