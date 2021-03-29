//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino sketch for the GMLAB UP4
// An open source Programmable USB Expression Pedal offering 4 CC types and 4 Response Curves
//
// Code by Guido Scognamiglio - www.GenuineSoundware.com
// Mar 2020
// 
// Runs on Arduino Leonardo or compatible boards (Atmel ATmega32U4)
// This sketch requires external libraries:
// - MIDI
// - MIDIUSB
//

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// External libraries
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <MIDI.h>
#include <MIDIUSB.h>
MIDI_CREATE_DEFAULT_INSTANCE();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pin definitions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PIN_POT   A0
#define PIN_D1    19 // A1
#define PIN_D2    20 // A2
#define PIN_D3    21 // A3
#define PIN_D4     4 // A6
#define PIN_D5     6 // A7
#define PIN_D6     8 // A8
#define PIN_D7     9 // A9
#define PIN_D8    10 // A10

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global variables
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// To create your custom curve, check this: https://www.genuinesoundware.com/products/_dev/curve.htm
byte ExpCurve[128] = { 0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 17, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 38, 39, 41, 42, 43, 44, 46, 47, 48, 49, 51, 52, 53, 54, 56, 57, 58, 59, 61, 62, 63, 64, 66, 67, 68, 69, 73, 76, 79, 81, 84, 87, 89, 92, 95, 98, 100, 103, 106, 108, 111, 114, 116, 119, 122, 124, 127 };

byte CCnumber[4] = { 11, 4, 2, 7 };
byte CCselect;
byte Channel;
byte CurveSelect;
int prev_value = -1;
#define DEADBAND 8

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS...
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Called to send events to the USB Midi output
void UsbMidiSend(byte b1, byte b2, byte b3)
{
  byte Status  = b1 & 0xF0;
  byte Channel = b1 & 0x0F;
  midiEventPacket_t Event;
  Event = {Status >> 4, Status | Channel, b2, b3};
  MidiUSB.sendMIDI(Event);
  MidiUSB.flush();
}

void DoPedal()
{
  int value = analogRead(PIN_POT);
  
  // Get difference from current and previous value
  int diff = abs(value - prev_value);
  
  // Exit this function if the new value is not within the deadband
  if (diff <= DEADBAND) return;
  
  // Store new value
  prev_value = value;    

  // Get the 7 bit value
  byte val7bit = value >> 3;

  // Now select the output value according to the selected scale
  byte cc_value;
  switch(CurveSelect)
  {
    // Linear
    case 0: cc_value = val7bit;                 break;
    // Exponential
    case 1: cc_value = ExpCurve[val7bit];       break;
    // Reverse Linear
    case 2: cc_value = 127 - val7bit;           break;
    // Reverse Exponential
    case 3: cc_value = 127 - ExpCurve[val7bit]; break;
  }
  
  // Send Midi 
  UsbMidiSend(0xB0 | Channel, CCnumber[CCselect], cc_value);
}


void setup() 
{
  pinMode(PIN_D1, INPUT_PULLUP);
  pinMode(PIN_D2, INPUT_PULLUP);
  pinMode(PIN_D3, INPUT_PULLUP);
  pinMode(PIN_D4, INPUT_PULLUP);
  pinMode(PIN_D5, INPUT_PULLUP);
  pinMode(PIN_D6, INPUT_PULLUP);
  pinMode(PIN_D7, INPUT_PULLUP);
  pinMode(PIN_D8, INPUT_PULLUP);

  // Init the MIDI Library
  MIDI.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages
  MIDI.turnThruOff(); // Turn soft-thru off
}

void loop() 
{
  // Read the DIP-Switch values
  Channel = 
      !digitalRead(PIN_D1) 
    | !digitalRead(PIN_D2) << 1 
    | !digitalRead(PIN_D3) << 2 
    | !digitalRead(PIN_D4) << 3;
  
  CCselect =     
      !digitalRead(PIN_D5) 
    | !digitalRead(PIN_D6) << 1;

  CurveSelect =    
      !digitalRead(PIN_D7) 
    | !digitalRead(PIN_D8) << 1; 

  // Read the ADC and generate the MIDI output
  DoPedal();  
}
