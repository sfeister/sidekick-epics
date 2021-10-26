/*
ShutterSCPI.ino

When externally triggered, opens the shutter for a specified time duration.

Important Notes:
 * Shutter begins its opening motion about 1000 microseconds after the external trigger is received.
 * Shutter begins its closing motion after the specified time duration from the beginning of its opening motion.
 * In other words: if the time duration is set too short, the shutter may never fully open!
 * External trigger should be wired to Arduino Pin 2.
 * Photodiode signal should be wired to Arduino Pin A0.


Serial Commands (lower-case portions are optional):
  *IDN?                 Responds with a device identification string.
  SHUtter:DURation VAL    Sets shutter-open time duration to VAL (unsigned long integer, in microseconds).
  SHUtter:DURation?       Responds with shutter-open time duration (unsigned long integer, in microseconds).

References:
 1. Following timer instructions at: https://github.com/contrem/arduino-timer
 2. Following Vrekrer SCPI Parser examples, e.g. at https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/examples/Numeric_suffixes/Numeric_suffixes.ino
 3. Attaching an interrupt pin for external triggering: https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/ 

Created by Emiko Ito, Keily Valdez-Sereno, and Scott Feister of California State University Channel Islands in Summer/Fall 2021.
*/

#include "Arduino.h"                                      // Arduino libraries
#include <arduino-timer.h>                                // Timing libraries, for managing trigger inputs and shutter timing
#include <math.h>                                         // Math libraries, for unsigned long to string conversion
#include "Vrekrer_scpi_parser.h"                          // SCPI libraries, for sending commands over USB
#include "Servo.h"                                        // Servo libraries, for controlling the shutter motor

#define EXTTRIG 2 // Note that this pin must be one of the Arduino pins capable of digital interrupt. See table at https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
#define SERVOPIN 3  // Controls the Servo on Arduino Pin 3

Timer<2, micros> timer1; // Timer with 2 task slots, microsecond resolution, and handler argument type void

SCPI_Parser my_instrument;                                //starts the SCPI command library
Servo myservo;                                            //creates a Servo called "myservo"
unsigned long duration1 = 500000; // duration of the shutter being open, in microseconds
unsigned long t0;

/* Shutter control functions */
void shutterClose() {
  // Close the shutter  
  myservo.write(0);
}

void shutterOpen() {
  // Open the shutter
  myservo.write(90);
}

/* Timing functions */
// Interrupt service routine (ISR): Called upon external trigger.
// Starts one-time timers that govern the open/close sequence of the shutter
void myISR() {
    // Schedule acquisition from the photodiode for the duration specified
    if (timer1.size() < 1) { // Only start a new shutter open/close sequence if the old one is completed
      t0 = micros() + 1000; // Set "shutter time-zero" to one thousand microseconds into the future to get everything set up first

      // Schedule the acquisition starts and stops
      timer1.at(t0, shutterOpen, 0); // Set the start time for the shutter to open
      timer1.at(t0 + duration1, shutterClose, 0); // Set the stop time for the shutter to close (must be significantly after the start time)
    }
}

/* Serial communication functions */
void identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {      //identifying what object you're using
    interface.println(F("DolphinDAQ,Arduino Shutter,#00,v0.1"));             //printing this line to the Serial Monitor
}


void getDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(duration1);
}

void setDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) { 
  if(parameters.Size() > 0) {
    duration1 = strtoul(parameters[0], NULL, 0);
  }
}

void setup() {
  my_instrument.RegisterCommand(F("*IDN?"), &identify);     //first command "*IDN?"
  my_instrument.SetCommandTreeBase(F("SHUtter:"));            //beginning to all other commands falls under the "SHU"tter tree
  my_instrument.RegisterCommand(F("DURation"), &setDuration);    //sets time duration of the open shutter
  my_instrument.RegisterCommand(F("DURation?"), &getDuration);    //gets time duration of the open shutter
  
  myservo.attach(SERVOPIN);                               //connects the name 'myservo' to the pin the Servo is attached to

  attachInterrupt(digitalPinToInterrupt(EXTTRIG), myISR, RISING); // Set up external triggering
  
  Serial.begin(9600);                                     //begins the Serial.monitor sequence
}

void loop() {
  timer1.tick();
  my_instrument.ProcessInput(Serial, "\r\n");   //starts allowing the Serial Monitor input for SCPI use
}
