// midi_out.h — envoi MIDI vers le MX49 (TX2 → 74AHCT125 → MIDI OUT du shield)
#pragma once
#include <Arduino.h>

namespace midiout {

void noteOn(uint8_t note, uint8_t velocite, uint8_t canal = 1);
void noteOff(uint8_t note, uint8_t canal = 1);
// Relâche toutes les notes envoyées et encore tenues (pause, stop, saut de position)
void toutCouper();

}  // namespace midiout
