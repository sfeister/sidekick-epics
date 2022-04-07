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
  MEASurement:DATa?           Responds with the most recent integration measurement (double, in Volts-seconds), along with its trigger count, as a formatted string
                              The format of this response is "DAT: XXXXXX, TRIG: XXXXXXX", where DAT: gives the data and TRIG: gives the trigger count
  SYStem:TRIGCount VAL        Sets the current system trigger count (unsigned long integer). Value is zero on startup and increments with each trigger input.
  SYStem:TRIGCount?           Responds with the current system trigger count (unsigned long integer).
  SYStem:DEBUG 0 | 1          Enables (1) or Disables (0) a debug print of many more variables on every acquisition. Disabled by default.
  SYStem:DEBUG?               Responds with 0 or 1, depending on if debug mode is enabled.

The system automatically streams the data to serial with every acquisition. The format of this unsolicited message is "STREAM DAT: XXXXXX, TRIG: XXXXXXX", where DAT: gives the data and TRIG: gives the trigger count.

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
unsigned long duration_daq;
double integration = 0; // resultant integration value following photodiode measurement
unsigned long sum, sum_daq; // temporary sum used during integration
double avg_daq, avg_volts;
unsigned long ncounts, ncounts_daq; // temporary number of ADC counts during integration
// unsigned long dt = 200; // microseconds between consecutive ADC measurements; must be large enough that it happens reliably on-time (keep above ~200 microseconds)
double dt;
unsigned long trigcnt = 0; // trigger count
unsigned long trigcnt_daq = 0; // trigger count of the active acquisition
unsigned long trigcnt_dat = 0; // trigger count of the integrated data
unsigned long areadmax = 1023; // analog read maximum value (1023 for the 10-bit ADC on a typical Arduino)
double areadmaxvolts = 5; // Analog read voltage corresponding to maximum value (5 V for a typical Arduino) - see https://www.arduino.cc/en/Reference/AnalogRead
bool ADCcontinue = false;
bool DAQIsReady = false; // true only when all is clear to begin a new acquisition
unsigned long t0;
String datMsgPrefix = "DAT: ";
String datMsgMiddle = ", TRIG: ";
String datMsg;
bool debugmode = false;

SCPI_Parser my_instrument;

/* Data acquisition functions */
bool DAQStart(void *argument) {
    // Begin data acquisition from the photodiode
    sum = 0; // reset ADC sum
    ncounts = 0; // reset number of ADC counts
    return false; // to repeat the action - false to stop
}

bool DAQStop(void *argument) {
    // End data acquisition from the photodiode and compute the integral
    sum_daq = sum; // grab the current ADC sum
    ncounts_daq = ncounts; // grab the current number of ADC counts
    trigcnt_dat = trigcnt_daq;

    if (ncounts_daq > 0) {
        avg_daq = double(sum_daq) / double(ncounts_daq); // average ADC value during acquisition  
    } else {
        avg_daq = 0;
    }
    
    avg_volts = avg_daq * (areadmaxvolts / areadmax); // average voltage during acquisition (converted from averaged ADC value)
    integration =  avg_volts * (duration_daq * 1e-6); // Compute integration value from ADC average in the DAQ period, in Volt-seconds
    // integration = sum_daq; // Debug only
    
    datMsg = String("STREAM ") + datMsgPrefix + String(integration, 8) + datMsgMiddle + String(trigcnt_dat); // Create an atomic data message which contains both trigger count and the data itself
    Serial.println(datMsg); // Streaming data print

    if (debugmode) {
      dt = duration_daq / ncounts_daq;
      Serial.println("DURATION_DAQ: " + String(duration_daq) + " us"); // Streaming data print
      Serial.println("DURATION1: " + String(duration1) + " us"); // Streaming data print
      Serial.println("DT: " + String(dt) + " us"); // Streaming data print
      Serial.println("AVG_DAQ: " + String(avg_daq, 8)); // Streaming data print
      Serial.println("AVG_VOLTS: " + String(avg_volts) + " V"); // Streaming data print
      Serial.println("AVG_DAQ: " + String(avg_daq, 8)); // Streaming data print
      Serial.println("NCOUNTS: " + String(ncounts_daq)); // Streaming data print    
    }
    DAQIsReady = true; // allow another acquisition to occur
    return false; // to repeat the action - false to stop
}

/* Timing functions */
// Interrupt service routine (ISR): Called upon external trigger.
// Starts one-time timers that govern the start/stop pulse of the ADC acquisition period
void myISR() {
    // Increment global trigger count (whether or not we use the data)
    trigcnt = trigcnt + 1;
    
    // Schedule acquisition from the photodiode for the duration specified
    if (timer1.size() < 1 && DAQIsReady) { // Only start a new acquisition if the old one is completed
      DAQIsReady = false;
      trigcnt_daq = trigcnt;      
      t0 = micros() + 1000; // Set "acquisition time-zero" to one thousand microseconds into the future to get everything set up first

      // Schedule the acquisition starts and stops
      duration_daq = duration1; // grab current duration value and use for this acquisition
      timer1.at(t0, DAQStart, 0); // Set the start time for the photodiode ADC data acquisition
      timer1.at(t0 + duration_daq, DAQStop, 0); // Set the stop time for the photodiode ADC data acquisition (must be significantly after the start time)
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

void getData(SCPI_C commands, SCPI_P parameters, Stream& interface) { 
  interface.println(datMsg); // Print the data
}


void getTrigCount(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(trigcnt);
}

void setTrigCount(SCPI_C commands, SCPI_P parameters, Stream& interface) { 
  if(parameters.Size() > 0) {
    trigcnt = strtoul(parameters[0], NULL, 0);
  }
}

void getDebugMode(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(debugmode);
}

void setDebugMode(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if(parameters.Size() > 0) {
    debugmode = String(parameters[0]).toInt();
  }
}

void setup() {
  my_instrument.RegisterCommand(F("*IDN?"), &identify);
  my_instrument.SetCommandTreeBase(F("MEASurement:"));
  my_instrument.RegisterCommand(F("DURation"), &setDuration);
  my_instrument.RegisterCommand(F("DURation?"), &getDuration);
  my_instrument.RegisterCommand(F("DATa?"), &getData);
  my_instrument.SetCommandTreeBase(F("SYStem:"));
  my_instrument.RegisterCommand(F("TRIGCount"), &setTrigCount);
  my_instrument.RegisterCommand(F("TRIGCount?"), &getTrigCount);
  my_instrument.RegisterCommand(F("DEBUG"), &setDebugMode);
  my_instrument.RegisterCommand(F("DEBUG?"), &getDebugMode);
  
  Serial.begin(9600);
  
  attachInterrupt(digitalPinToInterrupt(EXTTRIG), myISR, RISING); // Set up external triggering
  DAQIsReady = true;
}

void loop() {
  timer1.tick();
  my_instrument.ProcessInput(Serial, "\r\n");
  sum = sum + analogRead(PHOTOPIN);
  ncounts++;
}
