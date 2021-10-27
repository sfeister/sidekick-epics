/*
LEDsSCPI.ino

When externally triggered, flashes six LEDs a single time. Each LED has its own flash duration and brightness.

Important Notes:
 * LED flash begins abbout 1000 microseconds after the external trigger is received.
 * External trigger should be wired to Arduino Pin 2.
 * The six LED outputs should be wired to Arduino Pins 3, 5, 6, 9, 10, and 11.
 
Serial Commands (lower-case portions are optional):
  *IDN?                 Responds with a device identification string.
  DURation:LEDN VAL     Sets LED N (0-5) pulse duration to VAL (unsigned long integer, in microseconds).
  DURation:LEDN?        Responds with LED N (0-5) pulse duration (unsigned long integer, in microseconds).
  BRIGhtness:LEDN VAL   Sets LED N (0-5) brightness (PWM duty cycle) to VAL (integer, 0 to 255).
  BRIGhtness:LEDN?      Responds with LED N (0-5) brightness (PWM duty cycle)(integer, 0 to 255).
  
References:
 1. Following timer instructions at: https://github.com/contrem/arduino-timer
 2. Following Vrekrer SCPI Parser examples, e.g. at https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/examples/Numeric_suffixes/Numeric_suffixes.ino
 3. Attaching an interrupt pin for external triggering: https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/ 

Created by Keily Valdez-Sereno, Emiko Ito, and Scott Feister of California State University Channel Islands in Summer/Fall 2021.
*/

#include "Arduino.h" 
#include <arduino-timer.h>
#include "Vrekrer_scpi_parser.h"



#define EXTTRIG 2 // Note that this pin must be one of the Arduino pins capable of digital interrupt. See table at https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/

// Note that LEDs must be assigned to PWM-capable digital output pins
#define NLED 6 // Total number of LED outputs, each with independent duration and PWM brightness 
#define LED0 3
#define LED1 5
#define LED2 6
#define LED3 9
#define LED4 10
#define LED5 11

Timer<16, micros, int> timer1; // Timer with 16 task slots, microsecond resolution, and handler argument type int

int LEDPins[NLED]; // Arduino pin numbers for each of the six LED outputs
int brights[NLED]; // Brightnesses for each of the six LEDs, when on
unsigned long durations[NLED]; // Pulse duration times, in microseconds, for each of the six LEDs
unsigned long t0;

SCPI_Parser my_instrument;
int brightness = 0; // temp variable
int value = 0; // temp variable

/* LED control functions */
bool LEDStart(int LEDid) {
    // Turn on the LED of this index (0-5), with a specified PWM brightness
    analogWrite(LEDPins[LEDid], brights[LEDid]);
    return false; // to repeat the action - false to stop
}

bool LEDStop(int LEDid) {
    // Turn off the LED of this index (0-5)
    digitalWrite(LEDPins[LEDid], LOW);
    return false; // to repeat the action - false to stop
}

/* Timing functions */

// Interrupt service routine (ISR): Called upon external trigger.
// Starts one-time timers that govern the start/stop pulse of the LEDs
void myISR() {
    // Pulse the LEDs once, each with its own brightness and duration
    if (timer1.size() < 1) { // Only start a new set of LED pulses if the old set is completed
      t0 = micros() + 1000; // Set "LED pulse time zero" to one thousand microseconds into the future to get everything set up first

      // Schedule the LED pulse starts and stops
      for(int i = 0; i < NLED; ++i)
      {
          timer1.at(t0, LEDStart, i); // Set the start time for this LED
          timer1.at(t0 + durations[i], LEDStop, i); // Set the stop time for this LED (must be significantly after the start time)
      }
    }
}

/* Serial communication functions */
void identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(F("DolphinDAQ,Arduino Triggered LEDs,#00,v0.1"));
}

void getBrightness(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[LED]%u", &suffix);

  //If the suffix is valid, respond with the LED's duration
  if ( (suffix >= 0) && (suffix < NLED) ) {
     interface.println(String(brights[suffix], DEC));
  }
}

void setBrightness(SCPI_C commands, SCPI_P parameters, Stream& interface) { 
  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[LED]%u", &suffix);

  //If the suffix is valid,
  //use the first parameter (if valid) to set the LED brightness
  if ( (suffix >= 0) && (suffix < NLED) ) {
    if(parameters.Size() > 0) {
      value = String(parameters[0]).toInt();
      brights[suffix] = constrain(value, 0, 255);
    }
  }
}

void getDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[LED]%u", &suffix);

  //If the suffix is valid, respond with the LED's duration
  if ( (suffix >= 0) && (suffix < NLED) ) {
     interface.println(durations[suffix]);
  }
}

void setDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) { 
  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[LED]%u", &suffix);

  //If the suffix is valid,
  //use the first parameter (if valid) to set the LED duration
  if ( (suffix >= 0) && (suffix < NLED) ) {
    if(parameters.Size() > 0) {
      //durations[suffix] = strtoul(String(parameters[0]).toCharArray(buf, len));
      durations[suffix] = strtoul(parameters[0], NULL, 0);
    }
  }
}

void setup() { 
  my_instrument.RegisterCommand(F("*IDN?"), &identify); 
  my_instrument.SetCommandTreeBase(F("BRIGhtness:"));
  my_instrument.RegisterCommand(F("LED#?"), &getBrightness);
  my_instrument.RegisterCommand(F("LED#"), &setBrightness);
  my_instrument.SetCommandTreeBase(F("DURation:"));
  my_instrument.RegisterCommand(F("LED#?"), &getDuration);
  my_instrument.RegisterCommand(F("LED#"), &setDuration);
  
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);
  digitalWrite(LED0, LOW); 
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  digitalWrite(LED5, LOW);

  // Save pin numbers into the chanPins array
  LEDPins[0] = LED0;
  LEDPins[1] = LED1;
  LEDPins[2] = LED2;
  LEDPins[3] = LED3;
  LEDPins[4] = LED4;
  LEDPins[5] = LED5;
    
  // Set initial LED pulse durations (in microseconds)
  durations[0] = 200000;
  durations[1] = 500000;
  durations[2] = 150000;
  durations[3] = 400000;
  durations[4] = 350000;
  durations[5] = 100000;

  // Set intial LED brightnesses
  brights[0] = 200;
  brights[1] = 200;
  brights[2] = 200;
  brights[3] = 200;
  brights[4] = 200;
  brights[5] = 200;

  // Set up external triggering
  attachInterrupt(digitalPinToInterrupt(EXTTRIG), myISR, RISING);

  // Begin accepting SCPI commands
  Serial.begin(9600);
}

void loop() {
  timer1.tick();
  my_instrument.ProcessInput(Serial, "\r\n");
} 
