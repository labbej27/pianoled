// midi_out.cpp — envoi MIDI, avec suivi des notes tenues pour pouvoir toutes les relâcher
//
// Plusieurs pistes d'un fichier peuvent jouer la même note en même temps ; tout part sur le
// même canal du MX49. Pour éviter qu'une voix reste coincée, on compte les Note On par
// touche : une nouvelle frappe relâche d'abord la précédente (une seule voix par touche),
// et le Note Off n'est envoyé que lorsque la dernière piste relâche la note.

#include "midi_out.h"
#include "midi_interface.h"

namespace midiout {

static uint8_t compte[16][128];  // [canal][note] : nombre de pistes qui tiennent la note
static uint16_t canauxUtilises;  // 1 bit par canal ayant reçu au moins une note

void noteOn(uint8_t note, uint8_t velocite, uint8_t canal) {
    note &= 0x7F;
    canal = constrain(canal, 1, 16);
    uint8_t& c = compte[canal - 1][note];
    if (c) MIDI.sendNoteOff(note, 0, canal);  // refrappe : une seule voix par touche
    MIDI.sendNoteOn(note, velocite, canal);
    if (c < 255) c++;
    canauxUtilises |= 1u << (canal - 1);
}

void noteOff(uint8_t note, uint8_t canal) {
    note &= 0x7F;
    canal = constrain(canal, 1, 16);
    uint8_t& c = compte[canal - 1][note];
    if (!c) return;
    if (--c == 0) MIDI.sendNoteOff(note, 0, canal);
}

void toutCouper() {
    for (uint8_t c = 0; c < 16; c++) {
        if (!(canauxUtilises & (1u << c))) continue;
        for (uint8_t n = 0; n < 128; n++) {
            if (compte[c][n]) MIDI.sendNoteOff(n, 0, c + 1);
        }
        // Filet de sécurité : pédale relâchée, puis All Notes Off
        MIDI.sendControlChange(64, 0, c + 1);
        MIDI.sendControlChange(123, 0, c + 1);
    }
    memset(compte, 0, sizeof compte);
    canauxUtilises = 0;
}

}  // namespace midiout
