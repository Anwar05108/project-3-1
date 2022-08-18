// Include Libraries
#include "Arduino.h"
#include "Buzzer.h"
#include "LiquidCrystal_PCF8574.h"
#include "LDR.h"
#include "HX711.h"
#include "Relay.h"

// Pin Definitions
#define BUZZER_PIN_SIG 2
#define LDR_PIN_SIG	A3
#define SCALE_PIN_DAT 4
#define SCALE_PIN_CLK 3
#define RELAY_5V_PIN_COIL1 5
#define LIGHT_PIN 6
#define SWITCH_PIN 7

// Global variables and defines
// There are several different versions of the LCD I2C adapter, each might have a different address.
// Try the given addresses by Un/commenting the following rows until LCD works follow the serial monitor prints. 
// To find your LCD address go to: http://playground.arduino.cc/Main/I2cScanner and run example.
// #define LCD_ADDRESS 0x3F
#define LCD_ADDRESS 0x27
// Define LCD characteristics
#define LCD_ROWS 2
#define LCD_COLUMNS 16
#define SCROLL_DELAY 150
#define BACKLIGHT 255
#define THRESHOLD_ldr   100

// object initialization
Buzzer buzzer(BUZZER_PIN_SIG);
LiquidCrystal_PCF8574 lcdI2C;
LDR ldr(LDR_PIN_SIG);
HX711 scale(SCALE_PIN_DAT, SCALE_PIN_CLK);
#define calibration_factor 110 //This value is obtained using the SparkFun_HX711_Calibration sketch https://learn.sparkfun.com/tutorials/load-cell-amplifier-hx711-breakout-hookup-guide?_ga=2.77038550.2126325781.1526891300-303225217.1493631967
Relay relay_5v(RELAY_5V_PIN_COIL1);
Switchable light(LIGHT_PIN);

// states
#define START_STATE 0 // the user is not sitting
#define ACTIVE_STATE 1  // the user is sitting
#define WAITING_STATE 2 // waiting for the user to sit down again

// constants
const float THRESHOLD_WEIGHT = 20;
const int THRESHOLD_PASSED_TIME = 10000;
const int WAITING_TIME = 10000;
const int WARNING_THRESHOLD_TIME = 15000;
const int BUZZER_THRESHOLD_TIME = 20000;
const int BUZZER_DURATION = 1000;
const int BUZZER_TIME_PERIOD = 10000;
const int LDR_THRESHOLD = 100;

// global variables
int currState;
long activeStateStartTime;
long waitingStateStartTime;
bool warning;
bool playBuzzer;
long prevLcdDisplayTime;

// Setup the essentials for your circuit to work. It runs first every time your circuit is powered with electricity.
void setup() 
{
    // Setup Serial which is useful for debugging
    // Use the Serial Monitor to view printed messages
    Serial.begin(9600);
    while (!Serial) ; // wait for serial port to connect. Needed for native USB
    Serial.println("Starting Arduino Uno...");
    
    // initialize the lcd
    lcdI2C.begin(LCD_COLUMNS, LCD_ROWS, LCD_ADDRESS, BACKLIGHT);

    lcdI2C.clear();
    lcdI2C.print("PLEASE WAIT...");
    lcdI2C.selectLine(2);
    lcdI2C.print("CLEAR ANY WEIGHT");

    prevLcdDisplayTime = millis();
    
    scale.set_scale(calibration_factor); 
    scale.tare(); //Assuming there is no weight on the scale at start up, reset the scale to 0

    pinMode(SWITCH_PIN, INPUT);

    currState = START_STATE; // start with initial state

    delay(5000);

    lcdI2C.clear();
}

// Main logic of your circuit. It defines the interaction between the components you selected. After setup, it runs over and over again, in an eternal loop.
void loop() 
{
    // Serial.print("Time: "); Serial.println(millis()/1000);
    // Serial.print("In ");
    // if (currState == START_STATE) {
    //     Serial.println("Start State");
    // }
    // else if (currState == ACTIVE_STATE) {
    //     Serial.println("Active State");
    // }
    // else {
    //     Serial.println("Waiting State");
    // }

    // get weight reading
    float weightReading = abs(scale.get_units());
    // Serial.print("Weight Reading: "); Serial.println(weightReading);

    warning = false;
    // if currently in initial state
    if (currState == START_STATE) {
        // and weight reading is greater than or equal to the threshold
        if (weightReading >= THRESHOLD_WEIGHT) {
            // change state to active state
            currState = ACTIVE_STATE;
            // save starting time
            activeStateStartTime = millis();
            // Serial.print("Start State -> Active State at "); Serial.println(activeStateStartTime/1000);
        }
        // else remain in start state
    }
    // else if in active state
    else if (currState == ACTIVE_STATE) {
        // get time passed since entering active state
        long currTime = millis();
        long passedTime = currTime - activeStateStartTime;
        // Serial.print("Passed Time in Active State: "); Serial.println(passedTime/1000);
        // and weight reading is less then the threshold
        if (weightReading < THRESHOLD_WEIGHT) {
            // if passed time is less then threshold
            if (passedTime < THRESHOLD_PASSED_TIME) {
                // go back to start state
                currState = START_STATE;
                // Serial.print("Active State -> Start State at "); Serial.println(currTime/1000);
            }
            // otherwise
            else {
                // go to waiting state
                currState = WAITING_STATE;
                // save start time
                waitingStateStartTime = millis();
                // Serial.print("Active State -> Waiting State at "); Serial.println(waitingStateStartTime/1000);
            }
        }
        // check is passed time has passed warning threshold
        else if (passedTime >= WARNING_THRESHOLD_TIME) {
            // set warning to true
            warning = true;
        }
    }
    // else if in waiting state
    else {
        // get time passed since entering waiting state
        long currTime = millis();
        long passedTime = currTime - waitingStateStartTime;
        // Serial.print("Passed Time in Waiting State: "); Serial.println(passedTime/1000);
        // if weight is greater than or equal to threshold
        if (weightReading >= THRESHOLD_WEIGHT) {
            // go back to active state
            currState = ACTIVE_STATE;
            // Serial.print("Waiting State -> Active State at "); Serial.println(currTime/1000);
        }
        // else if passed time is greater than waiting time
        else if (passedTime > WAITING_TIME) {
            // go to start state
            currState = START_STATE;
            // Serial.print("Waiting State -> Start State at "); Serial.println(currTime/1000);
        }
        // else remain in waiting state
    }

    // read ldr value and check if relay should be turned on
    int ldrVal = ldr.readAverage();
    // Serial.print("LDR Value: "); Serial.println(ldrVal);
    if (ldrVal < LDR_THRESHOLD && currState == ACTIVE_STATE) {
        relay_5v.on();
    }
    else {
        relay_5v.off();
    }

    // get state of the switch
    int switchState = digitalRead(SWITCH_PIN);
    // Serial.print("Switch State: "); Serial.println(switchState);

    // check if the buzzer should be played
    playBuzzer = false;
    if (warning) {
        // turn on buzzer if buzzer threshold time is reached
        long currTime = millis();
        long passedTime = currTime - activeStateStartTime;
        if (passedTime >= BUZZER_THRESHOLD_TIME) {
            long periodTime = (passedTime - BUZZER_THRESHOLD_TIME) % BUZZER_TIME_PERIOD;
            if (periodTime <= BUZZER_DURATION) {
                playBuzzer = true;
            }
        }
    }
    // play buzzer if switch is on
    (playBuzzer && switchState == HIGH) ? buzzer.on() : buzzer.off();

    // light status
    if (warning) {
        light.toggle();
    }
    else if (currState == WAITING_STATE) {
        light.on();
    }
    else {
        light.off();
    }

    // calculate elapsed time
    float elapsedTime = 0;
    if (currState == ACTIVE_STATE) {
        long currTime = millis();
        elapsedTime = (currTime - activeStateStartTime) / 1000.0;
    }
    // Serial.print("Elapsed Time: "); Serial.println(elapsedTime);

    // display on lcd
    long currTime = millis();
    long lcdDisplayelapsedTime = currTime - prevLcdDisplayTime;
    if (lcdDisplayelapsedTime >= 1000) {
        lcdI2C.clear();
        if (currState == START_STATE) {
            lcdI2C.setCursor(1, 0);
            lcdI2C.print("LET'S GET SOME");
            lcdI2C.setCursor(3, 1);
            lcdI2C.print("WORK DONE.");
        }
        else if (currState == ACTIVE_STATE) {
            lcdI2C.print("TIME: ");
            if (elapsedTime < 10) {
                lcdI2C.print(elapsedTime, 2); lcdI2C.print(" s");
            }
            else {
                int s = elapsedTime;
                int h = s / (60*60);
                s %=(60*60);
                int m = s / 60;
                s %= 60;

                // Serial.print("t = "); Serial.println(elapsedTime);
                // Serial.print("h = "); Serial.println(h);
                // Serial.print("m = "); Serial.println(m);
                // Serial.print("s = "); Serial.println(s);
        
                if (h < 10) {
                    lcdI2C.print(0);
                }
                lcdI2C.print(h);
                lcdI2C.print(":");
                if (m < 10) {
                    lcdI2C.print("0");
                }
                lcdI2C.print(m);
                lcdI2C.print(":");
                if (s < 10) {
                    lcdI2C.print("0");
                }
                lcdI2C.print(s);
            }

            if (warning) {
                lcdI2C.selectLine(2);
                lcdI2C.print("TAKE A BREAK!!!");
            }
        }
        else {
            long currTime = millis();
            long breakTime = currTime - waitingStateStartTime;
            long remainingTime = WAITING_TIME - breakTime;

            lcdI2C.print("BREAK REMAINING: ");
            lcdI2C.selectLine(2);
            lcdI2C.print(remainingTime / 1000);
            lcdI2C.print(" s");
        }

        prevLcdDisplayTime = currTime;
    }

    // delay(500);
}
