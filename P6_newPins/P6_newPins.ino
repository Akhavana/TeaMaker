// Pins
#define UPIN 12 //button input
#define BIN_3 33 //motor 2A
#define BIN_4 27 //motor 2B
#define BIN_1 26 //motor 1A
#define BIN_2 25 //motor 1B
// #define POT 15 `
#define ONE_WIRE_BUS 5
#define PUMP 19
#define HEATER 21
//  encoder.attachHalfQuad(39, 34);  ENCODERS
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 14;
#include <ESP32Encoder.h>
#include "HX711.h"
ESP32Encoder encoder1;
ESP32Encoder encoder2;
HX711 scale;
#include <OneWire.h>
#include <DallasTemperature.h>
#define TXD1 8
#define RXD1 7
int motor1 = 99;
int motor2 = 99;

// Temp sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temp = 0.0;

// Tea Variables (changeable by user)
int brewTime = 10; //seconds
int dispenseTime = 10; //seconds
int waterHI = 65; //celsius
int waterLO = waterHI - 1; // hysterisis
int teaMassTarget = 6; //grams
int waterMassTarget = 100; //grams
int cupsOfTea = 1;

// State Machine
byte state = 3; 
volatile bool brewedTeaFlag = false;
volatile bool teaDoneComplete = false;

// Motor Speed Control
int omegaSpeed = 0;
int omegaDes = 0;
int omegaMax = 20; 
int D = 0;
int dir = 1;
int potReading = 0;
int Kp = 10; 
int Ki = 5;
float error_sum = 0;

// Motor PWM
const int freq = 5000;
const int ledChannel_1 = 1;
const int ledChannel_2 = 2;
const int resolution = 8;
const int MAX_PWM_VOLTAGE = 255;
const int NOM_PWM_VOLTAGE = 150;
volatile int count = 0;                  // encoder count
volatile int count1 = 0;                  // encoder count
volatile int count2 = 0;                  // encoder count
volatile bool deltaT = false;            // check timer interrupt for encoder count
hw_timer_t * timer0 = NULL; // button debouncer
hw_timer_t* timer1 = NULL; // encoder count
hw_timer_t* timer2 = NULL; // brewing tea
hw_timer_t* timer3 = NULL; // dispensing tea
portMUX_TYPE timerMux0 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux2 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux3 = portMUX_INITIALIZER_UNLOCKED;
HardwareSerial mySerial(2);
int button = 10;


// %%% ISRs %%%%
void IRAM_ATTR onTime1() { //encoder speed readoutt
  portENTER_CRITICAL_ISR(&timerMux1);
  count1 = encoder1.getCount();
  encoder1.clearCount();
  count2 = encoder2.getCount();
  encoder2.clearCount();
  deltaT = true;
  portEXIT_CRITICAL_ISR(&timerMux1);
}

void IRAM_ATTR onTime2() {
  portENTER_CRITICAL_ISR(&timerMux2);
  brewedTeaFlag = true; // timer flag for brewing tea
  portEXIT_CRITICAL_ISR(&timerMux2);
  timerStop(timer2);
}

void IRAM_ATTR onTime3() {
  portENTER_CRITICAL_ISR(&timerMux3);
  teaDoneComplete = true; // timer flag for dispensing tea
  portEXIT_CRITICAL_ISR(&timerMux2);
  timerStop(timer3);
}


void setup() {
  Serial.begin(115200); 
  mySerial.begin(9600, SERIAL_8N1, RXD1, TXD1);  // UART setup
  Serial.println("ESP32 UART Receiver");
  ESP32Encoder::useInternalWeakPullResistors = puType::up; //for encoders
  encoder1.attachHalfQuad(34, 39); // Motor 1
  encoder2.attachHalfQuad(36, 4);  // Motor 2
  encoder1.setCount(0); 
  encoder2.setCount(0); 
  ledcAttach(BIN_1, freq, resolution); //motors
  ledcAttach(BIN_2, freq, resolution);
  ledcAttach(BIN_3, freq, resolution);
  ledcAttach(BIN_4, freq, resolution);
  // pinMode(POT, INPUT);
  pinMode(UPIN, INPUT); 
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  pinMode(PUMP, OUTPUT);
  pinMode(HEATER, OUTPUT);
  sensors.begin();
  
  timer1 = timerBegin(1000000);            // Encoder readout timer: Set timer frequency to 1Mhz
  timerAttachInterrupt(timer1, &onTime1);  // Attach onTimer1 function to our timer.
  timerAlarm(timer1, 10000, true, 0);      // 10000 * 1 us = 10 ms, autoreload true

  timer2 = timerBegin(1000000);            // Tea brewing
  timerAttachInterrupt(timer2, &onTime2); 
  timerAlarm(timer2, 1000000*brewTime, true, 0); 

  timer3 = timerBegin(1000000);            // Tea dispensing
  timerAttachInterrupt(timer3, &onTime3); 
  timerAlarm(timer3, 1000000*dispenseTime, true, 0); 
} 


// %%%%%%%%%%%%%%%%%%%%%%%% MAIN LOOP %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void loop() {
  switch (state) {

    case 1: // IDLE
      Serial.println("state 1: idling");
      CheckForButtonPress();
      tareScale();
      checkScale();
      keepHeat();

      if (button == 1) { //tea 1
        Serial.println("button press tea 1");
        motor1 = BIN_1;
        motor2 = BIN_2;
        state = 2;
      }

      else if (button == 2) { //tea 2
        Serial.println("button press tea 2");
        motor1 = BIN_3;
        motor2 = BIN_4;
        state = 2;
      }
      break;

    case 2: // DISPENSING LEAVES
      Serial.println("state 2: dispensing leaves");
      CheckForButtonPress();
      startAuger(motor1, motor2, button);
      //keepHeat();
        
      if (teaMassChecker()) {
        stopAuger();
        state = 4;
        tareScale();
      }
      break;

    case 3: // PRE-HEATING WATER
      Serial.println("state 3: pre-heating water");
      CheckForButtonPress();
      if (preHeat()) {
        state=1;
      }
      break;

    case 4: // DISPENSING WATER
    Serial.println("state 4: dispensing water");
    stopAuger();
    CheckForButtonPress();
    //keepHeat();
    button = 10;

    startWater();
    
    if (waterMassChecker()) {
      stopWater();
      state = 5;
      timerWrite(timer2, 0); // Reset timer count
      timerStart(timer2);
    }
    break;

    case 5: // BREWING TEA
      Serial.println("state 5: brewing tea");
      CheckForButtonPress();
      keepHeat();

      if (brewedTeaFlag) {
        brewedTeaFlag = false;
        state = 6;
        timerWrite(timer3, 0); // Reset timer count
        timerStart(timer3);
      }

      break;

    case 6: // DISPENSING TEA
      Serial.println("state 6: Tea done!!!");
      //CheckForButtonPress();
      //keepHeat();

      if (teaDoneComplete) {
        teaDoneComplete = false;
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

void startAuger(int motor1, int motor2, int button) { // outputs PWM value to given motor
   
  if (deltaT) { //encoder count flag (10ms)
    portENTER_CRITICAL(&timerMux1);
    deltaT = false;
    portEXIT_CRITICAL(&timerMux1);

    if (button==1) {
      omegaSpeed = count1;
    } else if (button ==2) {
      omegaSpeed = count2;
    } else {omegaSpeed=0;}
    
    omegaDes = 8;
    error_sum = error_sum + (omegaDes-omegaSpeed)/10;
    D = Kp*(omegaDes-omegaSpeed) +Ki*(error_sum);  // P/PI controller 
    
    if (D > MAX_PWM_VOLTAGE) { //speed safeguards
      D = MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    } else if (D < -MAX_PWM_VOLTAGE) {
      D = -MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    }

    D=125;

    if (D > 0) { //motor inputs by PWM
      ledcWrite(motor1, LOW);
      ledcWrite(motor2, D);
    } else if (D < 0) {
      ledcWrite(motor2, LOW);
      ledcWrite(motor1, -D);
    } else {
      ledcWrite(motor2, LOW);
      ledcWrite(motor1, LOW);
    }
    plotControlData();
  }
}

void startAuger1() { // outputs PWM value to motor 1

  if (deltaT) { //encoder count flag (10ms)
    portENTER_CRITICAL(&timerMux1);
    deltaT = false;
    portEXIT_CRITICAL(&timerMux1);

    omegaSpeed = count;
    // potReading = analogRead(POT);
    omegaDes = 10;
    error_sum = error_sum + (omegaDes-omegaSpeed)/10;
    D = Kp*(omegaDes-omegaSpeed)+Ki*(error_sum);  // P/PI controller 
    
    if (D > MAX_PWM_VOLTAGE) { //speed safeguards
      D = MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    } else if (D < -MAX_PWM_VOLTAGE) {
      D = -MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    }

    // D=150;

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

void startAuger2() { // outputs PWM value to motor 2
  
  if (deltaT) { //encoder count flag (10ms)
    portENTER_CRITICAL(&timerMux1);
    deltaT = false;
    portEXIT_CRITICAL(&timerMux1);

    omegaSpeed = count;
    // potReading = analogRead(POT);
    omegaDes = 10;
    error_sum = error_sum + (omegaDes-omegaSpeed)/10;
    D = Kp*(omegaDes-omegaSpeed); // +Ki*(error_sum);  // P/PI controller 
    
    if (D > MAX_PWM_VOLTAGE) { //speed safeguards
      D = MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    } else if (D < -MAX_PWM_VOLTAGE) {
      D = -MAX_PWM_VOLTAGE;
      error_sum -= (omegaDes-omegaSpeed)/10;
    }

    // D=150;

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

void plotControlData() { // serial plotter
  Serial.print("Speed:");
  Serial.print(omegaSpeed);
  Serial.print(" ");
  Serial.print("Desired_Speed:");
  Serial.print(omegaDes);
  Serial.print(" ");
  Serial.print("PWM_Duty/10:");
  Serial.println(D / 10);  //PWM is scaled by 1/10 to get more intelligible graph
}

void stopAuger() { // stop all motors
    ledcWrite(BIN_2, LOW);
    ledcWrite(BIN_1, LOW);
    ledcWrite(BIN_3, LOW);
    ledcWrite(BIN_4, LOW);
}

void startHeater() { // turns on heater
  digitalWrite(HEATER, HIGH);
}

void stopHeater() { // turns off heater
  digitalWrite(HEATER, LOW);
}

void startWater() { // starts pump
  digitalWrite(PUMP, HIGH);
}

void stopWater() { // stops pump
  digitalWrite(PUMP, LOW);
}

float readTemp() { // print and return temp reading
  sensors.requestTemperatures(); 
  temp = sensors.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.print("C  |  ");
  return temp;
}

signed int checkScale() { //outputs and returns load cell reading 
    if (scale.is_ready()) {
    scale.set_scale();
    delay(200);
    long reading = scale.get_units(10);
    Serial.println("Weight:");
    Serial.println(reading);
    return reading;
  } 
}

void tareScale() { // tares once
  scale.tare();
}

bool waterMassChecker() { // measures if correct mass of water has been dispensed
  if (checkScale() > 200*waterMassTarget) { //100g
    return true;
  }
  else {
    return false;
  }
}

bool teaMassChecker() { // measures if correct mass of tea leaves has been dispensed
    if (checkScale() > 200*teaMassTarget) { //6g
    return true;
  }
  else {
    return false;
  }
}

bool CheckForButtonPress() { // digital button
  if (mySerial.available()) {

    // Read data and display it
    String message = mySerial.readStringUntil('\n');
    Serial.println("Received: " + message);
    
    if (message.toInt() == 1) { //tea1
      Serial.println("received 1");
      button = 1;
      return true;
      
    } else if (message.toInt() == 2) { //tea1
      Serial.println("received 2");
      button = 2;
      return true;
      
    } else if (message.toInt() == 5) { //reset
      Serial.println("reset");
      state = 1;
      button = 10;
      stopAuger();
      timerStop(timer2);
      timerStop(timer3);

    } else if (message.toInt() == 4) { // skip state?
      Serial.println("skip");
      button = 10;
      stopAuger();
      if (state==6) {
        state = 1;
      } 
      else if (state==3) {
        state = 1;
      }
      else if (state ==2) {
        state =4;
      }
      else {
        state = state + 1;
      }
      timerStop(timer2);
      timerStop(timer3);
    }

    else {
      return false; 
    } 
  }
}


bool preHeat() { //preheat water to desired temp
  if (readTemp() < waterHI) {
    startHeater();
    return false;
  } else {
    return true;
  }
}

void keepHeat() { //hysterisis within 1C
  if (readTemp() > waterHI) {
    stopHeater();
  } 
  else if (readTemp() < waterLO) {
    startHeater();
  }
}



  
  

