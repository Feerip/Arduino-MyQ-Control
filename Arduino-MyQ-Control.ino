//****************************************************
//  Author: Phillip Smith
//  Date: 9/26/2021
//  Email: feerip@gmail.com
//  Last Edit: 10/27/2021
//
//  This program, running on an Arduino, serves
//  as a middleman between the gate state output
//  and the radio chip in the MyQ garage door sensor
//  package, bypassing the PIC microcontroller
//  and accelerometer completely. It instructs the 
//  radio to transmit state (opened or closed) based
//  on the status reported by the gate, instead of 
//  the tilt reported by the accelerometer. 
//****************************************************
//Input pins, change these as you see fit
//Note - input must be 3.3v
//************************************
#define INPUT_PIN 8
#define MANUAL_OVERRIDE_PIN 99
#define OUTPUT_PIN 4
//************************************

//This program interfaces with the MyQ Door State Sensor Radio Chip (Si4010C2, 14-pin)
//Si4010C2 datasheet: https://www.silabs.com/documents/public/data-sheets/Si4010.pdf
//Sequence that the MyQ sensor radio chip expects on GPIO1(Pin13):
//DEFAULT STATE: HIGH
//WAKEUP: LOW 36ms
//PAUSE: HIGH 16ms
//BROADCAST TYPE: OPENED/CLOSED 534ms/134ms LOW
//BACK TO DEFAULT STATE: HIGH

//Timings - don't change these
#define WAKEUP 36
#define PAUSE 16
#define OPENED 534
#define CLOSED 134
//The timings are /r/oddlyspecific, I know.

//Keeps track of the last known gate state 
//so we know when to broadcast
int gate_state;

void setup() 
{  
  // put your setup code here, to run once:
  
  //initializes serial communication with 9600 baud rate
  Serial.begin(9600);
  while(!Serial){}
  Serial.print("Arduino MyQ Control V1.1\n");
  Serial.print("Written by Phillip Smith\n");
  Serial.print("Compiled with C++ version: ");
  Serial.print(__cplusplus);
  Serial.print("\n");
  Serial.print("Initializing...\n");

  //set initial gate state, assuming 
  //signal from gate is high
  gate_state = 1;
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
  //initializes the manual override input with
  //built-in pullup resistor
  pinMode(MANUAL_OVERRIDE_PIN, INPUT_PULLUP);

  Serial.print("...done\n");
}

void loop() {

  //read input indirectly, through debounce function
  int newState = readInput();

  //check for -1 to skip if debounce detected error
  if ((newState != gate_state) && (newState != -1))
  {
    if (newState == 0)
    {
      //turn on the LED immediately when 
      //state change (open) is detected
      digitalWrite(LED_BUILTIN, HIGH);
      //send the open signal
      broadcast(OPENED);
      Serial.print("Gate reports OPEN\n");
    }
    if (newState == 1)
    {
      //turn off the LED immediately when
      //state change (close) is detected      
      digitalWrite(LED_BUILTIN, LOW);
      //send the close signal
      broadcast(CLOSED);
      Serial.print("Gate reports CLOSED\n");
    }
    //state change was successful, so we save the state before 
    //starting the loop over
    gate_state = newState;
  }
}

//protocol for the Si4010C2's wakeup and pause sections is the same
//only difference is the broadcast type (signal LOW duration in ms)
//which is passed in here
void broadcast(int ms)
{
  //wakeup
  digitalWrite(OUTPUT_PIN, LOW);
  delay(WAKEUP);

  //pause
  digitalWrite(OUTPUT_PIN, HIGH);
  delay(PAUSE);

  //signal
  digitalWrite(OUTPUT_PIN, LOW);
  delay(ms);

  //finished, "you can go ahead and broadcast now"
  digitalWrite(OUTPUT_PIN, HIGH);
}

//this function serves as a simple debouncer for the input
//whether from button for testing or from the gate
int readInput()
{
  //if either of these are not changed from 
  //the default -1, that means error
  int read1 = -1;
  int read2 = -1;

  //initial read
  if (digitalRead(INPUT_PIN) == HIGH || digitalRead(MANUAL_OVERRIDE_PIN) == HIGH)
    read1 = 1;
  else if (digitalRead(INPUT_PIN) == LOW || digitalRead(MANUAL_OVERRIDE_PIN) == LOW)
    read1 = 0;
  //50ms debounce
  delay(50);
  //second read
  if (digitalRead(INPUT_PIN) == HIGH || digitalRead(MANUAL_OVERRIDE_PIN) == HIGH)
    read2 = 1;
  else if (digitalRead(INPUT_PIN) == LOW || digitalRead(MANUAL_OVERRIDE_PIN) == LOW)
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
