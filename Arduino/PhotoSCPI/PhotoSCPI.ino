/*
PhotoSCPI.ino

When externally triggered, integrates a photodiode signal for a specified time duration.

Important Notes:
 * Repeatedly acquires ADC values (every 200 us) from a photodiode input for a specified time period
 * Data acquisition (DAQ) begins about 1000 microseconds after the external trigger is received, and continues for the specified duration.
 * When duration is complete, an integration value in Volt-seconds is computed.
 * This integration is the only measurement value obtainable via Serial commands.
 * External trigger should be wired to Arduino Pin 2.
 * Photodiode signal should be wired to Arduino Pin A0.

Serial Commands (lower-case portions are optional):
  *IDN?                       Responds with a device identification string.
  MEASurement:DURation VAL    Sets photodiode ADC integration duration to VAL (unsigned long integer, in microseconds).
  MEASurement:DURation?       Responds with photodiode ADC integration pulse duration (unsigned long integer, in microseconds).
  MEASurement:VALue?          Responds with the integration measurement (double, in Volts-seconds) from the most recent acquisition.

References:
 1. Following timer instructions at: https://github.com/contrem/arduino-timer
 2. Following Vrekrer SCPI Parser examples, e.g. at https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/examples/Numeric_suffixes/Numeric_suffixes.ino
 3. Attaching an interrupt pin for external triggering: https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/ 

Created by Emiko Ito, Keily Valdez-Sereno, and Scott Feister of California State University Channel Islands in Summer/Fall 2021.
*/

#include "Arduino.h"
#include <math.h>
#include <arduino-timer.h>
#include "Vrekrer_scpi_parser.h"

#define EXTTRIG 2 // Note that this pin must be one of the Arduino pins capable of digital interrupt. See table at https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
#define PHOTOPIN A0 // Arduino Pin for Photodiode Input

Timer<3, micros> timer1; // Timer with 3 task slots, microsecond resolution, and handler argument type void

unsigned long duration1 = 400000; // duration of the photodiode measurement, in microseconds
double integration = 0; // resultant integration value following photodiode measurement
unsigned long sum = 0; // temporary sum used during integration
unsigned long dt = 200; // microseconds between consecutive ADC measurements; must be large enough that it happens reliably on-time (keep above ~200 microseconds)
unsigned long areadmax = 255; // analog read maximum value (255 for the 8-bit ADC on a typical Arduino)
double areadmaxvolts = 3.3; // Analog read voltage corresponding to maximum value (3.3 V for a typical Arduino)
bool ADCcontinue = false;
unsigned long t0;

SCPI_Parser my_instrument;

/* Data acquisition functions */
bool DAQRead(void *argument) { // Called repeatedly in short time intervals -- keep as simple as possible
    sum = sum + analogRead(PHOTOPIN);
    return ADCcontinue; // true to repeat the action - false to stop
}
    
bool DAQStart(void *argument) {
    // Begin data acquisition from the photodiode
    sum = 0;
    ADCcontinue = true;
    timer1.every(dt, DAQRead);
    return false; // to repeat the action - false to stop
}

bool DAQStop(void *argument) {
    // End data acquisition from the photodiode and compute the integral
    ADCcontinue = false;
    integration = (sum * dt) * (areadmaxvolts / areadmax) * 1e-6; // Compute integration value from sum, in Volt-seconds
    return false; // to repeat the action - false to stop
}

/* Timing functions */
// Interrupt service routine (ISR): Called upon external trigger.
// Starts one-time timers that govern the start/stop pulse of the ADC acquisition period
void myISR() {
    // Schedule acquisition from the photodiode for the duration specified
    if (timer1.size() < 1) { // Only start a new acquisition if the old one is completed
      t0 = micros() + 1000; // Set "acquisition time-zero" to one thousand microseconds into the future to get everything set up first

      // Schedule the acquisition starts and stops
      timer1.at(t0, DAQStart, 0); // Set the start time for the photodiode ADC data acquisition
      timer1.at(t0 + duration1, DAQStop, 0); // Set the stop time for the photodiode ADC data acquisition (must be significantly after the start time)
    }
}

/* Serial communication functions */
void identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(F("DolphinDAQ,Arduino Photodiode,#00,v0.1"));
}

void getDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(duration1);
}

void setDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) { 
  if(parameters.Size() > 0) {
    duration1 = strtoul(parameters[0], NULL, 0);
  }
}

void getValue(SCPI_C commands, SCPI_P parameters, Stream& interface) { 
  interface.println(integration, 8); // Print to eight decimal places
}

void setup() {
  my_instrument.RegisterCommand(F("*IDN?"), &identify);
  my_instrument.SetCommandTreeBase(F("MEASurement:"));
  my_instrument.RegisterCommand(F("DURation"), &setDuration);
  my_instrument.RegisterCommand(F("DURation?"), &getDuration);
  my_instrument.RegisterCommand(F("VALue?"), &getValue);

  attachInterrupt(digitalPinToInterrupt(EXTTRIG), myISR, RISING); // Set up external triggering

  Serial.begin(9600);
}

void loop() {
  timer1.tick();
  my_instrument.ProcessInput(Serial, "\r\n");
}
