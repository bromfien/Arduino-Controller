//digitalWriteFast version 1.2.0
#include <digitalWriteFast.h>

const byte runningPin = 10;
const byte interruptPin = 2;
const byte analogPin = 0;
const byte BIT[] = {0b00000011, 0b00001100, 0b00110000, 0b11000000};
const byte ZERO = 0;

const unsigned long SLEEP_TIME = 250;

volatile unsigned long start_time = 0;
volatile byte light_intensity = 0XFF; //256

volatile bool start_flag = false;
volatile bool data_read_flag = false;

void setup() {
  // #################################################################################
  // put your setup code here, to run once:
  // #################################################################################

  // initialize digital pin LED_BUILTIN as an output
  pinMode(LED_BUILTIN, OUTPUT);

  // initialize digital output pin 0 as an ouput
  pinMode(analogPin, OUTPUT);

  // initialize digital output pin XXXX as an ouput
  pinMode(runningPin, OUTPUT);

  // initialize pin 2 as an input with the pull up resistor enabled
  pinModeFast(interruptPin, INPUT_PULLUP);

  // attach pin 2 to both a rising and falling interupt
  attachInterrupt(digitalPinToInterrupt(interruptPin), edgeDetection, CHANGE);
  
}//end setup

void loop() {
  // #################################################################################
  // put your main code here, to run repeatedly: 
  // #################################################################################

  // BLOCK030 AND BLOCK080
  // BLOCK050
  if (((millis() - start_time) > (3 * SLEEP_TIME) && data_read_flag == false) ||
      ((millis() - start_time) > (9 * SLEEP_TIME) && data_read_flag == true)){

    start_flag = false;
    data_read_flag = false;

    // BLOCK006
    if (digitalReadFast(interruptPin) == LOW){

      analogWrite (analogPin, ZERO);
      digitalWrite(runningPin, LOW);

    }
    // BLOCK005 AND BLOCK008
    else {

      analogWrite (analogPin, light_intensity);

      if (light_intensity > 0)
      {
        digitalWrite(runningPin, HIGH);
      }
      else {
        digitalWrite(runningPin, LOW);
      }

    }

  } //end if

} // end loop

void edgeDetection() {
  // #################################################################################
  // code to run when a falling or rising edge is triggered:
  // #################################################################################

  if (digitalReadFast(interruptPin) == HIGH) {

    //RISING EDGE CODE:

    // BLOCK010
    // BLOCK070
    if ((start_flag == false && data_read_flag == false) ||
        (start_flag == true  && data_read_flag == false)){

      start_flag = true;
      light_intensity = 0;
      start_time = millis();

      //BLOCK001 AND BLOCK002
      //analogWrite (analogPin, light_intensity);

    } //if

    // BLOCK040
    else if (start_flag == true && data_read_flag == true){

      unsigned long elapse_time = millis() - start_time;

      if (elapse_time > SLEEP_TIME * 1.5 && elapse_time < SLEEP_TIME * 2.5){
        light_intensity += BIT[0]; 
      }
      else if (elapse_time > SLEEP_TIME * 3.5 && elapse_time < SLEEP_TIME * 4.5){
        light_intensity += BIT[1]; 
      }
      else if (elapse_time > SLEEP_TIME * 5.5 && elapse_time < SLEEP_TIME * 6.5){
        light_intensity += BIT[2]; 
      }
      else if (elapse_time > SLEEP_TIME * 7.5 && elapse_time < SLEEP_TIME * 8.5){
        light_intensity += BIT[3]; 
      }
    } //else if

    digitalWrite(LED_BUILTIN, HIGH);

  } 

  else{

    // BLOCK020
    if (start_flag == true && data_read_flag == false){

      data_read_flag = true;

    }
    // BLOCK060
    else if (start_flag == false && data_read_flag == false){

      start_flag = true;
      start_time = millis();

    }

    digitalWrite(LED_BUILTIN, LOW);

  }
  
} // end edgeDetection