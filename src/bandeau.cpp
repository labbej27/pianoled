// bandeau.cpp — pilotage FastLED et correspondance note → LED
#include "bandeau.h"

namespace bandeau {

static CRGB tampon[cfg::NB_LEDS_MAX];  // image envoyée au bandeau
static CRGB fond[cfg::NB_LEDS_MAX];    // couche de fond
static uint8_t velocites[cfg::NB_NOTES];
static CRGB lecture[cfg::NB_NOTES];
static CRGB flashCouleur[cfg::NB_NOTES];
static uint32_t flashFin[cfg::NB_NOTES];  // 0 = pas de flash
static bool montrerJouees = true;
static CLEDController* controleur = nullptr;
static uint16_t nbActuel = cfg::NB_LEDS_MAX;
static bool modifie = true;
static uint32_t dernierEnvoi = 0;
constexpr uint32_t INTERVALLE_MIN_MS = 10;       // ~100 images/s au maximum
constexpr uint32_t RAFRAICHISSEMENT_MS = 50;     // renvoi périodique : une image ratée se corrige d'elle-même

void eteindreImmediatement() {
    controleur = &FastLED.addLeds<WS2812B, cfg::BROCHE_LEDS, GRB>(tampon, cfg::NB_LEDS_MAX);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, cfg::LIMITE_MA);
    FastLED.setBrightness(60);
    fill_solid(tampon, cfg::NB_LEDS_MAX, CRGB::Black);
    fill_solid(fond, cfg::NB_LEDS_MAX, CRGB::Black);
    FastLED.show();  // on éteint les NB_LEDS_MAX LED, au cas où le bandeau serait plus long
    nbActuel = cfg::NB_LEDS_MAX;
}

void appliquerReglages() {
    const uint16_t n = reglages.nb_leds;
    if (n < nbActuel) {
        // Éteint les LED qui ne seront plus adressées
        fill_solid(tampon, nbActuel, CRGB::Black);
        FastLED.show();
    }
    controleur->setLeds(tampon, n);
    nbActuel = n;
    FastLED.setBrightness(reglages.luminosite);
    modifie = true;
}

// ---------- Correspondance ----------

float positionNote(int indexNote) {
    return reglages.offset_mm + indexNote * reglages.pas_note_mm;
}

static int16_t versIndexPhysique(long i) {
    const long n = reglages.nb_leds;
    if (i < 0 || i >= n) return -1;
    return reglages.sens_inverse ? (int16_t)(n - 1 - i) : (int16_t)i;
}

LedsNote ledsDepuisPosition(float pos_mm) {
    const float f = pos_mm / cfg::PAS_LED_MM;
    const long base = lroundf(f);
    LedsNote r{versIndexPhysique(base), -1};
    if (reglages.leds_par_note == 2) {
        // La voisine est celle du côté où tombe le centre de la note
        const long voisine = (f >= (float)base) ? base + 1 : base - 1;
        r.b = versIndexPhysique(voisine);
    }
    return r;
}

LedsNote ledsDeNote(int indexNote) {
    if (indexNote < 0 || indexNote >= cfg::NB_NOTES) return {-1, -1};
    return ledsDepuisPosition(positionNote(indexNote));
}

// ---------- Couche de fond ----------

void effacerFond() {
    fill_solid(fond, cfg::NB_LEDS_MAX, CRGB::Black);
    modifie = true;
}

void fondLed(int idx, const CRGB& c) {
    if (idx < 0 || idx >= reglages.nb_leds) return;
    fond[idx] = c;
    modifie = true;
}

void fondLeds(const LedsNote& l, const CRGB& c) {
    fondLed(l.a, c);
    fondLed(l.b, c);
}

void fondTout(const CRGB& c) {
    fill_solid(fond, reglages.nb_leds, c);
    modifie = true;
}

// ---------- Couche du morceau ----------

void noteLecture(int indexNote, const CRGB& c) {
    if (indexNote < 0 || indexNote >= cfg::NB_NOTES || lecture[indexNote] == c) return;
    lecture[indexNote] = c;
    modifie = true;
}

void effacerLecture() {
    fill_solid(lecture, cfg::NB_NOTES, CRGB::Black);
    modifie = true;
}

void flash(int indexNote, const CRGB& c, uint16_t duree_ms) {
    if (indexNote < 0 || indexNote >= cfg::NB_NOTES) return;
    flashCouleur[indexNote] = c;
    flashFin[indexNote] = millis() + duree_ms;
    if (!flashFin[indexNote]) flashFin[indexNote] = 1;
    modifie = true;
}

void afficherNotesJouees(bool actif) {
    if (montrerJouees == actif) return;
    montrerJouees = actif;
    modifie = true;
}

// ---------- Couche des notes jouées ----------

void noteJouee(int indexNote, uint8_t velocite) {
    if (indexNote < 0 || indexNote >= cfg::NB_NOTES) return;
    velocites[indexNote] = velocite;
    modifie = true;
}

void toutesNotesJoueesEteintes() {
    memset(velocites, 0, sizeof velocites);
    modifie = true;
}

// ---------- Composition et envoi ----------

void miseAJour() {
    const uint32_t t = millis();
    for (int i = 0; i < cfg::NB_NOTES; i++) {
        if (flashFin[i] && (int32_t)(t - flashFin[i]) >= 0) {
            flashFin[i] = 0;
            modifie = true;
        }
    }
    const uint32_t ecoule = t - dernierEnvoi;
    if (ecoule < INTERVALLE_MIN_MS) return;
    if (!modifie && ecoule < RAFRAICHISSEMENT_MS) return;
    dernierEnvoi = t;
    modifie = false;

    memcpy(tampon, fond, sizeof(CRGB) * reglages.nb_leds);
    for (int i = 0; i < cfg::NB_NOTES; i++) {
        CRGB c = lecture[i];
        if (montrerJouees && velocites[i]) {
            // Vert, plus intense quand la vélocité est forte
            c = CRGB(0, 30 + (uint16_t)velocites[i] * 225 / 127, 0);
        }
        if (flashFin[i]) c = flashCouleur[i];
        if (!c) continue;  // noir : on laisse le fond
        const LedsNote l = ledsDeNote(i);
        if (l.a >= 0) tampon[l.a] = c;
        if (l.b >= 0) tampon[l.b] = c;
    }
    FastLED.show();
}

}  // namespace bandeau
