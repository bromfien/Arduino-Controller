// digitalWriteFast version 1.2.0
#include <digitalWriteFast.h>

// Ticker version 4.4.0
// Includes the Ticker library to allow scheduling functions to run periodically or with delays
#include <Ticker.h>

// Function prototypes
void EdgeDetectionInterrupt();
void SignalStartProcessingComplete();
void DataProcessingComplete();
void MicrosToTime(char* t_return_string, unsigned long t_current_micros);
void UpdateBuildInLED();
void UpdateRunningLED();
unsigned long Diff(unsigned long t_higher_number, unsigned long t_lower_number);
void PrintMessage(int t_index, unsigned long t_current_time);

// ########################## Constants ######################
// RUNNING_PIN: Pin number for the output that controls the running LED.
const byte RUNNING_LED_PIN = 9;
// INTERRUPT_PIN: Pin number for the interrupt input that detects signal edges.
const byte INTERRUPT_PIN = 3;

// Each index represents the 4 bits used to encode the light intensity value.
// Bit 0 is the LSB. Bit 3 is the MSB.
const byte BIT_PATTERN[] = { 0b00000011, 0b00001100, 0b00110000, 0b11000000 };

// SIGNAL_WIDTH_TIME: The width in microseconds of the signal pulse from the camera relay.
const unsigned long SIGNAL_WIDTH_TIME = 250000; //250ms

// Global Enum
// MessageFlags enum defines message flag values used in inter-thread messaging.
enum MessageFlags {
  END_SEND_MESSAGE = 0,       //indicates end of message sending.
  END_DATA_READ_MESSAGE = 1,  //indicates end of data read from sensor.
  RELAY_CLOSE_MESSAGE = 2,    //signals relay should be closed.
  RELAY_OPEN_MESSAGE = 3,     //signals relay should be opened.
  WRONG_EDGE = 4              //indicates that back to back rising or falling edges were detected   
};

// ###################### Global variables ######################
volatile unsigned long start_time = 0;
volatile byte current_light_intensity = 0XFF;  //256
volatile byte updated_light_intensity = 0X00;

// Flag used to prevent case where back-to-back rising or falling edge
// detection occurs without the opposite edge occuring first. I am not sure why this is happening.
bool edge_verification_flag = false;

// 4 flags prevent a second rising trigger that occurs inside the window from being double counted.
// This is only required if the camera relay doesn't following the 250ms bit width.
bool bit_flag[] = {false, false, false, false};

// Update built in led every 10 ms
Ticker built_in_led_timer(UpdateBuildInLED, 10); 

// Changing led every 1 seconds. Timer is stopped when the first interrupt is detected on the INTERRUPT_PIN.
Ticker running_led_timer(UpdateRunningLED, 1000); 

// Timer start after first signal change in a data packet
Ticker signal_start_timer(SignalStartProcessingComplete, 3*SIGNAL_WIDTH_TIME/1000); 

// Timer for completing signal processing 
// 11 * SIGNAL_WIDTH_TIME allows full processing of 4 bits 
Ticker data_processing_timer(DataProcessingComplete, 11*SIGNAL_WIDTH_TIME/1000);

/* 
 * setup code here, to run once:
*/
void setup() {
        // initialize digital pin LED_BUILTIN as an output
        pinMode(LED_BUILTIN, OUTPUT);

        // initialize analog output pin 0 as an output
        pinMode(DAC, OUTPUT);

        // initialize digital output pin 9 as an ouput
        pinMode(RUNNING_LED_PIN, OUTPUT);

        // initialize pin 3 as an input with the pull up resistor enabled
        pinModeFast(INTERRUPT_PIN, INPUT_PULLUP);

        // attach pin 3 to both a rising and falling interupt
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), EdgeDetectionInterrupt, CHANGE);

        // open the serial port at 115200 bps:
        Serial.begin(115200);

        // start the timer. Timer runs continuously.
        built_in_led_timer.start();

        // start the timer. Timer is stopped when the first interrupt is detected on the INTERRUPT_PIN.
        running_led_timer.start();
}// End setup

/*
 * The loop function runs repeatedly after setup() completes. 
 * It calls the update() method on each of the timer objects 
 * to keep them running and check if they need to trigger any actions.
*/
void loop() {
        built_in_led_timer.update();
        running_led_timer.update();
        signal_start_timer.update();
        data_processing_timer.update();
} // End loop function

/*
 * code to run when a falling or rising edge is triggered:
 * edge detection is monitoring interruptPin for a change in state 
 * 
 * Camera close NO relay is logical 1
 * Camera open NO relay is logical 0
 * Sourcing input pulls the input to ground (LOW) when the relay is closed.
 * Input is at 5V (HIGH) when the relay is open.
 *
 * Interrupt service routine for detecting edges on the interrupt pin. 
 * Handles starting timers on edge detection, decoding light intensity 
 * signal, and updating LED based on new intensity.
*/
void EdgeDetectionInterrupt() {
        const unsigned long current_time = micros();

        //stops the the running LED from 
        running_led_timer.stop();

        if (digitalReadFast(INTERRUPT_PIN) == LOW && !edge_verification_flag) {
            //############################### Relay Closed ####################################
            edge_verification_flag = !edge_verification_flag;

            // BLOCK010
            // BLOCK070
            if (data_processing_timer.state() == STOPPED) {
                start_time = current_time;

                signal_start_timer.start();

                updated_light_intensity = 0;

                for (unsigned int i = 0; i < sizeof(bit_flag)/sizeof(bit_flag[0]); i++) {
                    bit_flag[i] = false;
                }
                //BLOCK001 AND BLOCK002
                analogWrite(DAC, current_light_intensity);
                digitalWrite(RUNNING_LED_PIN, current_light_intensity > 0 ? HIGH : LOW);
            }
            // BLOCK040
            else if (signal_start_timer.state() == PAUSED  && data_processing_timer.state() == RUNNING) {
                const unsigned long elapsed_time = Diff(current_time, start_time);

                if (abs(elapsed_time - SIGNAL_WIDTH_TIME * 2) < SIGNAL_WIDTH_TIME * 0.5 && !bit_flag[0]) {
                    updated_light_intensity += BIT_PATTERN[0];
                    bit_flag[0] = true;
                    Serial.println (F("\t\t\t\t\t\t\t\t\t\t\tBIT 0"));
                }
                else if (abs(elapsed_time - SIGNAL_WIDTH_TIME * 4) < SIGNAL_WIDTH_TIME * 0.5 && !bit_flag[1]) {
                    updated_light_intensity += BIT_PATTERN[1];
                    bit_flag[1] = true;
                    Serial.println (F("\t\t\t\t\t\t\t\t\t\t\tBIT 1"));
                }
                else if (abs(elapsed_time - SIGNAL_WIDTH_TIME * 6) < SIGNAL_WIDTH_TIME * 0.5 && !bit_flag[2]) {
                    updated_light_intensity += BIT_PATTERN[2];
                    bit_flag[2] = true;
                    Serial.println (F("\t\t\t\t\t\t\t\t\t\t\tBIT 2"));
                }
                else if (abs(elapsed_time - SIGNAL_WIDTH_TIME * 8) < SIGNAL_WIDTH_TIME * 0.5 && !bit_flag[3]) {
                    updated_light_intensity += BIT_PATTERN[3];
                    bit_flag[3] = true;
                    Serial.println (F("\t\t\t\t\t\t\t\t\t\t\tBIT 3"));
                }
            }
            else {
                Serial.println("ERROR");
            }
            PrintMessage(RELAY_CLOSE_MESSAGE, current_time);
            //############################# End Relay Closed ##################################
        }
        else if (digitalReadFast(INTERRUPT_PIN) == HIGH && edge_verification_flag) {
            //################################ Relay Open #####################################
            edge_verification_flag = !edge_verification_flag;

            // BLOCK020
            if (signal_start_timer.state() == RUNNING && data_processing_timer.state() == STOPPED) {
                //pausing the timer prevents the timer from finishing but allows the paused state to be used later
                signal_start_timer.pause();

                data_processing_timer.start();
            }
            // BLOCK060
            else if (signal_start_timer.state() == STOPPED  && data_processing_timer.state() == STOPPED) {
                start_time = current_time;

                signal_start_timer.start();
            }
            PrintMessage(RELAY_OPEN_MESSAGE, current_time);
            //############################# End Relay Open ###################################
        }
        else {
            PrintMessage(WRONG_EDGE, current_time);
        }
} // End Function edgeDetection

/* 
 * Timer for signal_start_timer.
 * 3 width timer has expired and only a single rising or falling edge has occured
*/
void SignalStartProcessingComplete () {
        // BLOCK030 AND BLOCK080
        // ####################### t=3 if statement ####################################
        const unsigned long current_time = micros();

        signal_start_timer.stop();

        // BLOCK006
        if (digitalReadFast(INTERRUPT_PIN) == HIGH) {
            analogWrite(DAC, 0);
            digitalWrite(RUNNING_LED_PIN, LOW);
        }
        // BLOCK005
        else {
            analogWrite(DAC, current_light_intensity);
            digitalWrite(RUNNING_LED_PIN, current_light_intensity > 0 ? HIGH : LOW);
        }
        PrintMessage(END_SEND_MESSAGE, current_time);
        // ######################### end t=3 width if statement ########################
} // End SignalStartProcessingComplete function

/* 
 * Timer for data_processing_timer.
 * 11 width timer has expired and the updated light intensity will be updated based on
 * signal received.
*/
void DataProcessingComplete () {
        // BLOCK050
        // ############################ t=11 if statement ###############################
        const unsigned long current_time = micros();

        //signal_start_timer will be paused at this time
        signal_start_timer.stop();
        
        data_processing_timer.stop();

        current_light_intensity = updated_light_intensity;

        // BLOCK008
        if (digitalReadFast(INTERRUPT_PIN) == HIGH) {
            analogWrite(DAC, 0);
            digitalWrite(RUNNING_LED_PIN, LOW);
        }
        // BLOCK005
        else {
            analogWrite(DAC, current_light_intensity);
            digitalWrite(RUNNING_LED_PIN, current_light_intensity > 0 ? HIGH : LOW);
        }
        PrintMessage(END_DATA_READ_MESSAGE, current_time);
        // ########################## end t=11 if statement ###########################
} // End DataProcessingComplete function

/* 
 * converts the number of milliseconds since the program started to a string with the 
 * following format: HH:MM:SS.mmmmmm
*/
void MicrosToTime(char* t_return_string, unsigned long t_current_micros) {
        unsigned long microseconds = t_current_micros;
        unsigned long seconds = microseconds / 1000000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        microseconds %= 1000000;  //1E6
        seconds %= 60;
        minutes %= 60;
        sprintf(t_return_string, "%02lu:%02lu:%02lu.%06lu", hours, minutes, seconds, microseconds);
} // End microsToTime

/* 
 * controls the function of the built in LED.
*/
void UpdateBuildInLED() {
        //Saw tooth the builtInLED LED
        const unsigned int builtin_led_intensity = micros() / 6000 % 512;
        analogWrite(LED_BUILTIN, builtin_led_intensity > 255 ? 512 - builtin_led_intensity : builtin_led_intensity);
        //Toogle the builtInLED LED
        //digitalWriteFast (LED_BUILTIN, !digitalReadFast (LED_BUILTIN));
} // end function updateBuildInLED

/* 
 * controls the function of the running in LED before the calibration flag is run
*/
void UpdateRunningLED() {
        //Toogle the builtInLED LED
        digitalWriteFast(RUNNING_LED_PIN, !digitalReadFast(RUNNING_LED_PIN));
} // End function UpdateRunningLED

/* 
 * A function that takes two unsigned longs and returns the difference between the first and second.
 * It also handles the case where the second unsigned long has rolled over the maximum value unsigned long 
 * 
 * If first is greater than or equal to second, simply return first - second
 * Otherwise, first has rolled over the maximum value of unsigned long 
 * So the maximum value plus one is added to second and then subtract it from first
*/

unsigned long Diff(unsigned long t_higher_number, unsigned long t_lower_number) {
        return t_higher_number >= t_lower_number ? t_higher_number - t_lower_number : t_higher_number - (t_lower_number + UINT32_MAX + 1);
} // End function Diff

/* 
 * Function handles all the serial printing used during debugging.
*/
void PrintMessage(int t_index, unsigned long t_current_time) {
        char text[250];
        char date_text[16];
        MicrosToTime(date_text, t_current_time);
        switch (t_index) {
            case 0:
            {
                //END_SEND_MESSAGE
                const unsigned long elapsed_time = Diff(t_current_time, start_time);
                const float width_calc = elapsed_time / SIGNAL_WIDTH_TIME;
                sprintf(text, "End Send      \t %s \t start flag: %s \t data read flag: %s \t width: %.1f \t lag after trigger: %lu \t old light intensity: %02X", date_text, signal_start_timer.state() == RUNNING  ? "HIGH" : "LOW", data_processing_timer.state() == RUNNING ? "HIGH" : "LOW", width_calc, elapsed_time % SIGNAL_WIDTH_TIME, current_light_intensity);
            }break;
            case 1:
            {
                //END_DATA_READ_MESSAGE
                const unsigned long elapsed_time = Diff(t_current_time, start_time);
                const float width_calc = elapsed_time / SIGNAL_WIDTH_TIME;
                sprintf(text, "End Data Read \t %s \t start flag: %s \t data read flag: %s \t width: %.1f \t lag after trigger: %lu \t new light intensity: %02X", date_text, signal_start_timer.state() == RUNNING  ? "HIGH" : "LOW", data_processing_timer.state() == RUNNING ? "HIGH" : "LOW", width_calc, elapsed_time % SIGNAL_WIDTH_TIME, updated_light_intensity);
            }break;
            case 2:
            {
                //RELAY_CLOSE_MESSAGE
                sprintf(text, "Relay Close   \t %s \t start flag: %s \t data read flag: %s", date_text, signal_start_timer.state() == RUNNING  ? "HIGH" : "LOW", data_processing_timer.state() == RUNNING ? "HIGH" : "LOW");
            }break;
            case 3:
            {
                //RELAY_OPEN_MESSAGE
                sprintf(text, "Relay Open    \t %s \t start flag: %s \t data read flag: %s", date_text, signal_start_timer.state() == RUNNING  ? "HIGH" : "LOW", data_processing_timer.state() == RUNNING ? "HIGH" : "LOW");
            }break;
            case 4:
            {
                //WRONG_EDGE
                sprintf(text, "Wrong Edge   \t %s \t start flag: %s \t data read flag: %s", date_text, signal_start_timer.state() == RUNNING  ? "HIGH" : "LOW", data_processing_timer.state() == RUNNING ? "HIGH" : "LOW");
            }
            default:
            {
                // Should never reach here
                return;
            }
        }
        Serial.println(text);
} // End function printMessage