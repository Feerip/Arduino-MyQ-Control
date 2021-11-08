//****************************************************
//  Author: Phillip Smith
//  Date: 9/26/2021
//  Email: feerip@gmail.com
//
//  This program, running on an Arduino, serves
//  as a middleman between the gate state output
//  and the radio chip in the MyQ garage door sensor
//  package, bypassing the PIC microcontroller
//  and accelerometer completely. It instructs the 
//  radio to transmit state (opened or closed) based
//  on the status reported by the gate, instead of 
//  the tilt reported by the accelerometer. 
//
//  Date: 10/26/2021
//  Updates by Greydigger
//
//  Gate status time emulation for MYQ protocols,
//  
#define CLOSE_TO_OPEN_TIME 6000
#define OPEN_TO_CLOSED_TIME 13000
#define DEBOUNCE_TIME 50
// 5 minutes -
#define MIN_BROADCAST_PERIOD 300000
//
//****************************************************
//Input pins, change these as you see fit
//Note - input must be 3.3v
//************************************
#define INPUT_PIN 8
#define OUTPUT_PIN 4
#define OVERRIDE_1_PIN 3
#define OVERRIDE_2_PIN 2
//************************************

//This program interfaces with the MyQ Door State Sensor Radio Chip (Si4010C2, 14-pin)
//Si4010C2 datasheet: https://www.silabs.com/documents/public/data-sheets/Si4010.pdf
//Sequence that the MyQ sensor radio chip expects on GPIO1(Pin13):
//DEFAULT STATE: HIGH
//WAKEUP_PW: LOW 36ms
//PAUSE_PW: HIGH 16ms
//BROADCAST TYPE: OPENED_PW/CLOSED_PW 534ms/134ms LOW
//BACK TO DEFAULT STATE: HIGH

//Timings - don't change these
#define WAKEUP_PW 36
#define PAUSE_PW 16
#define OPENED_PW 534
#define CLOSED_PW 134
//The timings are /r/oddlyspecific, I know.

//Logic state definitions
#define GATE_OPEN 0
#define GATE_CLOSED 1
//Keeps track of the last known gate state 
//so we know when to broadcast
int gate_state;
int pairing_mode;

// need to periodically send gate status according to MyQ requirements
// global variable that gets reset by broadcast().
long idle_time = MIN_BROADCAST_PERIOD;
 
void setup() 
{  
  // put your setup code here, to run once:
  
  //initializes serial communication with 9600 baud rate
  Serial.begin(9600);
  while(!Serial){}
  Serial.print("Arduino MyQ Control V1.0\n");
  Serial.print("Written by Phillip Smith\n");
  Serial.print("Compiled with C++ version: ");
  Serial.print(__cplusplus);
  Serial.print("\n");
  Serial.print("Initializing...\n");

  //set initial gate state, assuming 
  //signal from gate is high
  gate_state = GATE_OPEN;
  pinMode(OUTPUT_PIN, OUTPUT);
  //sets the default state of the output pin, Si4010C2
  //expects a default 3.3v (HIGH)
  digitalWrite(OUTPUT_PIN, HIGH);
  //initializes the onboard LED, will be used when we think gate is open
  //for debugging physically at the gate
  pinMode(LED_BUILTIN, OUTPUT);
  //initializes the input (signal from gate) with 
  //built-in pullup resistor
  pinMode(INPUT_PIN, INPUT_PULLUP);
  //initialize override switch inputs (for assist with gate pairing process)
  pinMode(OVERRIDE_1_PIN, INPUT_PULLUP);
  pinMode(OVERRIDE_2_PIN, INPUT_PULLUP);

  // if "closed" override switch is detected on startup go to manual open/close timing mode
  pairing_mode = 0;
  if (digitalRead(OVERRIDE_1_PIN)==LOW) {
    pairing_mode = 1;                         
    Serial.print("override closed switch detected on startup, setting manual timing mode\n"); 
  }
  Serial.print("...done\n");
  // Serial.print(idle_time);

}

void loop() {

  //read input indirectly, through debounce function
  int newState = readInput();
    //check for -1 to skip if debounce detected error
  if ((newState != gate_state) && (newState != -1))
  {
    if (newState == GATE_OPEN)
    {
      //turn on the LED immediately when 
      //state change (open) is detected
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print("Gate open command detected...");
      if(!pairing_mode) delay(CLOSE_TO_OPEN_TIME);
      //send the "gate is open" signal
      broadcast(OPENED_PW);
      Serial.print("Gate reports OPEN\n");
    }
    if (newState == GATE_CLOSED)
    {
      //turn off the LED immediately when
      //state change (close) is detected      
      digitalWrite(LED_BUILTIN, LOW);
      Serial.print("Gate Close command detected...");
      if(!pairing_mode) delay(OPEN_TO_CLOSED_TIME);
      //send the "gate is closed" signal
      broadcast(CLOSED_PW);
      Serial.print("Gate reports CLOSED\n");
    }
    //state change was successful, so we save the state before 
    //starting the loop over
    gate_state = newState;
  } 
  else if (idle_time <= 0)
    {
      Serial.print("re-broadcasting gate status: ");
      Serial.print(idle_time);
      Serial.print("\n");
      broadcast(gate_state == GATE_CLOSED ? CLOSED_PW : OPENED_PW);
    }

}

//protocol for the Si4010C2's wakeup and pause sections is the same
//only difference is the broadcast type (signal LOW duration in ms)
//which is passed in here
void broadcast(int ms)
{
  //wakeup
  digitalWrite(OUTPUT_PIN, LOW);
  delay(WAKEUP_PW);

  //pause
  digitalWrite(OUTPUT_PIN, HIGH);
  delay(PAUSE_PW);

  //signal
  digitalWrite(OUTPUT_PIN, LOW);
  delay(ms);

  //finished, "you can go ahead and broadcast now"
  digitalWrite(OUTPUT_PIN, HIGH);

  // reset GLOBAL idle_time variable each time broadcase() is used;
  idle_time = MIN_BROADCAST_PERIOD;
}

//this function serves as a simple debouncer for the input
//whether from button for testing or from the gate
// 10/27/2021 added override switch inputs. logical precedence.
int readInput()
{
  //if either of these are not changed from 
  //the default -1, that means error
  int read1 = -1;
  int read2 = -1;

  //initial read
  if (digitalRead(OVERRIDE_1_PIN)==LOW)
    read1 = 1;
  else if (digitalRead(OVERRIDE_2_PIN)==LOW)
    read1 = 0;
  else if (digitalRead(INPUT_PIN) == HIGH)
    read1 = 1;
  else if (digitalRead(INPUT_PIN) == LOW)
    read1 = 0;
  //50ms debounce
  delay(DEBOUNCE_TIME);

  // GLOBAL VARIABLE USAGE
  idle_time -= DEBOUNCE_TIME;  

  //second read
  if (digitalRead(OVERRIDE_1_PIN)==LOW)
    read2 = 1;
  else if (digitalRead(OVERRIDE_2_PIN)==LOW)
    read2 = 0;
  else if (digitalRead(INPUT_PIN) == HIGH)
    read2 = 1;
  else if (digitalRead(INPUT_PIN) == LOW)
    read2 = 0;

  //if both reads were 1, the input is good.
  //pass it up the chain
  if ((read1 == 1) && (read2 == 1))
    return 1;
    
  //if both reads were 0, the input is good. 
  //pass it up the chain
  else if ((read1 == 0) && (read2 == 0))
    return 0;
  
  //if neither of the previous conditions were met, 
  //read was bad, pass error up the chain
  else
    return -1;
   
}
