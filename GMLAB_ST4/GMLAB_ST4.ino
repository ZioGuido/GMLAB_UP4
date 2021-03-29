//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino sketch for the GMLAB ST4
// An open source Do-It-Yourself 4-PORT SWITCHABLE MIDI THRU BOX
// A THRU box that can turn output ports on or off in a safe way, using 4 illuminated buttons.
// Checks that a messages has been sent completely before turning off a port, and uses "Note Memory"
// that remembers a note-on events and sends the corresponding note-off event even after a port has been turned off,
// in order to prevent stuck notes.
//
// Applications: use this box to control up to 4 synthesizers/modules with one single controller/keyboard and 
// use the buttons to choose which unit to control.
//
// Code by Guido Scognamiglio - www.GenuineSoundware.com
// Start: Aug 2020 - Last update: Oct 2020
// 
// Runs on Arduino Leonardo or compatible boards (Atmel ATmega32U4)
// This sketch requires external libraries:
// - MIDI
// - MIDIUSB
// - MillsTimer
//

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pin definitions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PIN_LED0  10
const char ButtonPins[4] = { 2, 3, 4, 5 };
const char ButtonLeds[4] = { 6, 7, 8, 9 };
const char MuxPins[4] = { 18, 19, 20, 21 };


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Other useful definitions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MIDI_STATUS_NOTEOFF           0x80  // b2=note, b3=velocity
#define MIDI_STATUS_NOTEON            0x90  // b2=note, b3=velocity
#define MIDI_STATUS_AFTERTOUCH        0xA0  // b2=note, b3=pressure
#define MIDI_STATUS_CONTROLCHANGE     0xB0  // b2=controller, b3=value
#define MIDI_STATUS_PROGRAMCHANGE     0xC0  // b2=program number
#define MIDI_STATUS_CHANNELPRESSURE   0xD0  // b2=pressure
#define MIDI_STATUS_PITCHBEND         0xE0  // pitch (b3 & 0x7f) << 7 | (b2 & 0x7f) and center=0x2000


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// External libraries
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <EEPROM.h>
#include <MIDI.h>
#include <MIDIUSB.h>
MIDI_CREATE_DEFAULT_INSTANCE();

#include "MillisTimer.h"  // This library creates a timer with millisecond precision
MillisTimer ButtonTimer;  // This is used for checking the buttons
MillisTimer ActivityTimer;  // Used to blink the Activity LED


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global variables
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char ButtonStatus[4] = { 1, 1, 1, 1 };

// This single variable will be used with bitshift operators so that one or more ports can be on or off
char PortStatus = 0; 
char USBPort = 0;

// Unfortunately, due to limited memory, the NoteMemory can only be applied to a single channel
struct NoteMemory
{
  char Status, Port;
};
NoteMemory NoteMem[128];
NoteMemory SustainPedal[16];


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS...
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Called to send events to the USB Midi output (linked to OUT 1)
void UsbMidiSend(byte b1, byte b2, byte b3)
{
  // USB output mirrors MIDI output #1
  if (!USBPort) return;
  
  byte Status  = b1 & 0xF0;
  byte Channel = b1 & 0x0F;
  Channel -= 1; // MIDIUSB Library wants channels in range 0~15
  midiEventPacket_t Event;
  Event = {Status >> 4, Status | Channel, b2, b3};
  MidiUSB.sendMIDI(Event);
  MidiUSB.flush();
}

// Pass the port variable that can either be the current PortStatus or an arbitrary port
void SetPort(char port)
{
  // Used to enable the USB Midi output linked to OUT 1
  USBPort = port & 1;
  
  // Switch ports
  for (int i=0; i<4; i++)
  {
    digitalWrite(MuxPins[i], (port >> i) & 1);
  }

  // Allow 10 uS for the MUX ports to stabilize
  delayMicroseconds(10);
}

void SwitchPorts(char pin)
{
  // Change bits with XOR operations
  switch (pin)
  {
  case 0: PortStatus ^= 1; break;
  case 1: PortStatus ^= 2; break;
  case 2: PortStatus ^= 4; break;
  case 3: PortStatus ^= 8; break;
  }

  // Set corresponding LED
  digitalWrite(ButtonLeds[pin], (PortStatus >> pin) & 1);
}

void DoButton(int btn, int status)
{
  if (ButtonStatus[btn] == status) return;
  ButtonStatus[btn] = status;

  // Things to do when a button is depressed
  if (status == LOW)
  {
    SwitchPorts(btn);
    EEPROM.update(8, PortStatus);
  }
}

void CheckButtons()
{
  for (int i=0; i<4; i++)
  {
    DoButton(i, digitalRead(ButtonPins[i]));
  }
}

void ActivityLed()
{
  digitalWrite(PIN_LED0, HIGH);
}

void setup() 
{
  // Turn on power LED
  pinMode(PIN_LED0, OUTPUT);
  ActivityLed();

  // Restore last state
  PortStatus = EEPROM.read(8);

  // Init I/Os
  for (int i=0; i<4; i++)
  {
    pinMode(ButtonPins[i], INPUT_PULLUP);
    pinMode(ButtonLeds[i], OUTPUT);
    pinMode(MuxPins[i], OUTPUT);
    digitalWrite(MuxPins[i], LOW);
    
    digitalWrite(ButtonLeds[i], (PortStatus >> i) & 1);
  }
  
  // Initialize the Note Memory
  for (int c=0; c<16; c++) // for each channel
  {
    SustainPedal[c].Status = 0;
    SustainPedal[c].Port = 0;
  }
    
  for (int n=0; n<128; n++) // for each note
  {
    NoteMem[n].Status = 0;
    NoteMem[n].Port = 0;
  }

  // Set Timers
  ButtonTimer.setInterval(5);
  ButtonTimer.expiredHandler(CheckButtons);
  ButtonTimer.start();

  ActivityTimer.setInterval(10);
  ActivityTimer.setRepeats(1); // One-shot timer
  ActivityTimer.expiredHandler(ActivityLed);

  // Init the MIDI Library
  MIDI.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages
  MIDI.turnThruOff(); // Turn soft-thru off
}

void loop() 
{
  // Read and process incoming MIDI messages
  if (MIDI.read())
  {
    // Start blinking Activity (Power) LED
    digitalWrite(PIN_LED0, LOW);
    ActivityTimer.start();

    switch (MIDI.getType())
    {
      case MIDI_STATUS_NOTEON:
        // Every time a NoteOn event is received, its status and port must be remembered
        NoteMem[MIDI.getData1()].Status = 1;
        NoteMem[MIDI.getData1()].Port = PortStatus;
        SetPort(PortStatus);
        MIDI.sendNoteOn(MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
        UsbMidiSend(MIDI_STATUS_NOTEON | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        break;

      case MIDI_STATUS_NOTEOFF:
        // When a NoteOff is received, the message is only sent to its own port(s) if this note was on
        if (NoteMem[MIDI.getData1()].Status)
        {
          NoteMem[MIDI.getData1()].Status = 0; // reset status
          SetPort(NoteMem[MIDI.getData1()].Port); // set appropriate port (override current setting)
          MIDI.sendNoteOff(MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
          UsbMidiSend(MIDI_STATUS_NOTEOFF | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        }
        break;

      case MIDI_STATUS_CONTROLCHANGE:
        // In case of CC#64 (Sustain Pedal)...
        if (MIDI.getData1() == 64)
        {
          if (MIDI.getData2() > 63) 
          {
            SustainPedal[MIDI.getChannel() - 1].Status = 1;
            SustainPedal[MIDI.getChannel() - 1].Port = PortStatus;
            SetPort(PortStatus);
            MIDI.sendControlChange(64, 127, MIDI.getChannel());
            UsbMidiSend(MIDI_STATUS_CONTROLCHANGE | MIDI.getChannel(),64, 127);
          }
          else
          {
            SustainPedal[MIDI.getChannel() - 1].Status = 0;
            SetPort(SustainPedal[MIDI.getChannel() - 1].Port);
            MIDI.sendControlChange(64, 0, MIDI.getChannel());
            UsbMidiSend(MIDI_STATUS_CONTROLCHANGE | MIDI.getChannel(), 64, 0);
          }
          
          break;
        }
        
        SetPort(PortStatus);
        MIDI.sendControlChange(MIDI.getData1(), MIDI.getData2(), MIDI.getChannel());
        UsbMidiSend(MIDI_STATUS_CONTROLCHANGE | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        break;

      case MIDI_STATUS_PROGRAMCHANGE:
        SetPort(PortStatus);
        MIDI.sendProgramChange(MIDI.getData1(), MIDI.getChannel());
        UsbMidiSend(MIDI_STATUS_PROGRAMCHANGE | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        break;

      case MIDI_STATUS_CHANNELPRESSURE:
        SetPort(PortStatus);
        MIDI.sendAfterTouch(MIDI.getData1(), MIDI.getChannel());
        UsbMidiSend(MIDI_STATUS_CHANNELPRESSURE | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        break;

      case MIDI_STATUS_PITCHBEND:
        SetPort(PortStatus);
        // 14 bit to dec (-8192 ~ 8192), center = 0
        MIDI.sendPitchBend((MIDI.getData1() + (MIDI.getData2() << 7) - 8192), MIDI.getChannel());
        UsbMidiSend(MIDI_STATUS_PITCHBEND | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
        break;
    }
  }

  // Run Timers
  ButtonTimer.run();
  ActivityTimer.run();
}
