// bandeau.h — pilotage du bandeau WS2812B et correspondance note → LED (mapping)
#pragma once
#include <FastLED.h>
#include "config.h"

namespace bandeau {

// LED(s) associées à une position ou une note ; -1 = hors bandeau / absente
struct LedsNote {
    int16_t a;
    int16_t b;
};

// À appeler en tout premier dans setup() : éteint le bandeau entier
void eteindreImmediatement();
// Applique nb_leds et luminosité après un changement de réglages
void appliquerReglages();

// --- Correspondance ---
float    positionNote(int indexNote);          // offset_mm + i * pas_note_mm
LedsNote ledsDepuisPosition(float pos_mm);     // LED la plus proche (+ voisine si 2 LED/note)
LedsNote ledsDeNote(int indexNote);            // indexNote = 0..48

// --- Couche de fond (tests, aperçu, calibration) ---
void effacerFond();
void fondLed(int idx, const CRGB& c);
void fondLeds(const LedsNote& l, const CRGB& c);
void fondTout(const CRGB& c);

// --- Couche du morceau en lecture (une couleur par touche ; noir = rien) ---
void noteLecture(int indexNote, const CRGB& c);
void effacerLecture();

// --- Couche des notes jouées au clavier (vert selon la vélocité) ---
void noteJouee(int indexNote, uint8_t velocite);  // vélocité 0 = relâchée
void toutesNotesJoueesEteintes();
void afficherNotesJouees(bool actif);  // masquée pendant la lecture d'un morceau

// --- Flash bref sur une touche (bonne note / mauvaise note), au-dessus de tout ---
void flash(int indexNote, const CRGB& c, uint16_t duree_ms);

// Ordre de superposition : fond < lecture < notes jouées < flash

// À appeler à chaque tour de loop() : compose et envoie l'image si elle a changé
void miseAJour();

}  // namespace bandeau
