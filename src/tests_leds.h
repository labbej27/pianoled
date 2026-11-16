// tests_leds.h — animations de diagnostic, non bloquantes (machine à états cadencée par millis())
#pragma once
#include <Arduino.h>

namespace tests {

void couleurs();                         // rouge, vert, bleu, 1 s chacun
void chenillardLeds(uint16_t ms);        // LED 0 .. nb_leds-1
void chenillardNotes(uint16_t ms);       // 49 notes avec la correspondance calculée
void gammeMidiOut();                     // gamme de do envoyée sur MIDI OUT
void apercu();                           // affichage fixe des 49 notes
void noteSeule(int indexNote);           // affichage fixe d'une note
void arreter();                          // stoppe l'animation et efface le fond
bool enCours();
void boucle();                           // à appeler à chaque tour de loop()

}  // namespace tests
