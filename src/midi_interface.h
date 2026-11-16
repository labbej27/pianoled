// midi_interface.h — instance MIDI partagée par midi_in et midi_out (UART2, RX 16 / TX 17)
#pragma once
#include <MIDI.h>

using InterfaceMidi = midi::MidiInterface<midi::SerialMIDI<HardwareSerial>>;
extern InterfaceMidi MIDI;  // définie dans midi_in.cpp
