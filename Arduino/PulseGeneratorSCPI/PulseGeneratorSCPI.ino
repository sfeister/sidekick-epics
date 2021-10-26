/*
PulseGeneratorSCPI.ino

Generates 3.3 V TTL square pulses at a specified repetition rate.

Important Notes:
 * Creates eight independently delayed TTL pulses. Each channel's delay time is specified in microseconds relative to TRIGOUT.
 * Each TTL pulse is emitted as HIGH (3.3 V) with a 3 millisecond duration, after which is goes back to its LOW state (0 V).
 * The overall repetition rate of the system can be adjusted up to about 100 Hz (depending on other time delay choices).
 * Typical precision of the requested delay times (on an Arduino Uno) is about 50 microseconds. This jitters around.
 * "Time-zero" trigger output can be monitored on Arduino Pin 3.
 * Up to eight channels (CH0 - CH7) of controlled TTL pulse outputs can be wired to Arduino Pins 4, 5, 6, 7, 8, 9, 10, 11.

Serial Commands (lower-case portions are optional):
  *IDN?                 Responds with a device identification string.
  DELay:CHannelN VAL    Sets Channel N (0-7) output delay to VAL (unsigned long integer, in microseconds).
  DELay:CHannelN?       Responds with output delay (unsigned long integer, in microseconds) of Channel N (0-7).
  REPrate VAL           Sets system-wide repetition-rate to VAL (double, in Hz).
  REPrate?              Responds with the system-wide repetition-rate (double, in Hz).

References:
 1. Following timer instructions at: https://github.com/contrem/arduino-timer
 2. Following Vrekrer SCPI Parser examples, e.g. at https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/examples/Numeric_suffixes/Numeric_suffixes.ino

Created by Scott Feister, Emiko Ito, and Keily Valdez-Sereno of California State University Channel Islands in Fall 2021.
*/

#include "Arduino.h"
#include <math.h>
#include <arduino-timer.h>
#include "Vrekrer_scpi_parser.h"

#define TRIGOUT 3 // The "zero delay" TRIGOUT TTL pulse is output on Arduino Pin 3
#define NCHAN 8 // Total number independently delayed output TTL pulse output channels
#define CH0 4 // Channel 0 TTL pulse is output on Arduino Pin 4
#define CH1 5 // Channel 1 TTL pulse is output on Arduino Pin 5
#define CH2 6 // Channel 2 TTL pulse is output on Arduino Pin 6
#define CH3 7 // Channel 3 TTL pulse is output on Arduino Pin 7
#define CH4 8 // Channel 4 TTL pulse is output on Arduino Pin 8
#define CH5 9 // Channel 5 TTL pulse is output on Arduino Pin 9
#define CH6 10 // Channel 5 TTL pulse is output on Arduino Pin 10
#define CH7 11 // Channel 5 TTL pulse is output on Arduino Pin 11

/* Serial communication initialization */
SCPI_Parser my_instrument;

/* Timing initialization */
Timer<10, micros, int> timer1; // TTL pulse timer with 10 task slots, microsecond resolution, and handler argument type int
Timer<1, millis> timer2; // rep-rate timer with 1 task slot, millisecond resolution, and void-type handler

int chanPins[NCHAN]; // Pins for each channel
int delays[NCHAN]; // Millisecond delay times for each channel

int pulseTime = 3000; // TTL pulse duration in microseconds
double repRate = 1; // pulse repetition-rate in Hz
unsigned long t0;

/* Timing functions */
bool TTLStop(int chanPin) {
    // Bring this pulse output back to LOW
    digitalWrite(chanPin, LOW);
    return false; // to repeat the action - false to stop
}

bool TTLStart(int chanPin) {
    // Bring this channel output to HIGH
    digitalWrite(chanPin, HIGH);
    timer1.in(pulseTime, TTLStop, chanPin); // Start a timer to bring this signal back to LOW
    return false; // to repeat the action - false to stop
}

bool trigStart(void *argument){
    if (timer1.size() < 1) { // Only start a new set of triggers if the old set is completed
      t0 = micros() + 1000; // Set "time zero" two thousand microseconds into the future to get everything set up first

      // Schedule the "TRIGOUT" / t0 pulse
      timer1.at(t0, TTLStart, TRIGOUT);
      
      // Schedule the channel pulses
      for(int i = 0; i < NCHAN; ++i)
      {
          timer1.at(t0 + delays[i], TTLStart, chanPins[i]); // Start a timer for this channel
      }
    }
    return true; // repeat at the next interval of the timer
}

void updateRepRate(double repRateHz){
  // Sets the repetition-rate, with value in Hz
  timer2.cancel();
  int repTime = round(1000 / repRateHz);
  timer2.every(repTime, trigStart); // Start a timer for this channel
  repRate = repRateHz; // Update the global variable "repRate"
}

/* Serial communication functions */
void identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(F("DolphinDAQ,Arduino TTL Pulse Generator,#00,v0.1"));
}


void getRepRate(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(repRate);
}

void setRepRate(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  String first_parameter = String(parameters.First());
  updateRepRate(first_parameter.toDouble());
}

void getDelay(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // This function is modified from the Vrekrer examples: https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/examples/Numeric_suffixes/Numeric_suffixes.ino
  
  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[CHANNEL]%u", &suffix);

  //If the suffix is valid, print the channel's value to the interface
  if ( (suffix >= 0) && (suffix < NCHAN) ) {
      interface.println(delays[suffix]);
  }
}

void setDelay(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // This function is modified from the Vrekrer examples: https://github.com/Vrekrer/Vrekrer_scpi_parser/blob/master/examples/Numeric_suffixes/Numeric_suffixes.ino
  
  //Get the numeric suffix/index (if any) from the commands
  String header = String(commands.Last());
  header.toUpperCase();
  int suffix = -1;
  sscanf(header.c_str(),"%*[CHANNEL]%u", &suffix);

  //If the suffix is valid,
  //use the first parameter (if valid) to set the channel delay
  String first_parameter = String(parameters.First());
  if ( (suffix >= 0) && (suffix < NCHAN) ) {
      delays[suffix] = first_parameter.toInt();
  }
}


void setup() {
  /* Serial communication setup */
  my_instrument.RegisterCommand(F("*IDN?"), &identify);
  my_instrument.RegisterCommand(F("REPrate?"), &getRepRate);
  my_instrument.RegisterCommand(F("REPrate"), &setRepRate);
  my_instrument.SetCommandTreeBase(F("DELay:"));
  my_instrument.RegisterCommand(F("CHannel#?"), &getDelay);
  my_instrument.RegisterCommand(F("CHannel#"), &setDelay);

  /* Timing setup */
  pinMode(TRIGOUT, OUTPUT); // Declare the TRIGOUT pin as a digital output
  pinMode(CH0, OUTPUT);
  pinMode(CH1, OUTPUT);
  pinMode(CH2, OUTPUT);
  pinMode(CH3, OUTPUT);  
  pinMode(CH4, OUTPUT);
  pinMode(CH5, OUTPUT);
  pinMode(CH6, OUTPUT);
  pinMode(CH7, OUTPUT);

  // Save pin numbers into the chanPins array
  chanPins[0] = CH0;
  chanPins[1] = CH1;
  chanPins[2] = CH2;
  chanPins[3] = CH3;
  chanPins[4] = CH4;
  chanPins[5] = CH5;
  chanPins[6] = CH6;
  chanPins[7] = CH7;
    
  // Save initial delay times (in microseconds) into the delays array
  delays[0] = 2000;
  delays[1] = 2000;
  delays[2] = 2000;
  delays[3] = 2000;
  delays[4] = 2000;
  delays[5] = 2000;
  delays[6] = 2000;
  delays[7] = 2000;

  // Initialize the repetition rate
  updateRepRate(repRate); // Sets the repetition-rate, with value in Hz

  Serial.begin(9600);
}

void loop() {
  timer1.tick();
  timer2.tick();
  my_instrument.ProcessInput(Serial, "\r\n")
}
