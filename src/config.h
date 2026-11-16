// config.h — constantes matérielles et réglages persistants (NVS)
#pragma once
#include <Arduino.h>

namespace cfg {
// Broches (câblage vérifié, voir README)
constexpr uint8_t BROCHE_LEDS    = 4;   // D4 → 74AHCT125 canal 1 → 220 Ω → DIN du bandeau
constexpr uint8_t BROCHE_MIDI_RX = 16;  // RX2 ← shield MIDI (pont diviseur 1k/2,2k)
constexpr uint8_t BROCHE_MIDI_TX = 17;  // TX2 → 74AHCT125 canal 2 → shield MIDI

constexpr uint32_t VITESSE_SERIE = 115200;
constexpr uint32_t VITESSE_MIDI  = 31250;

// Bandeau WS2812B 144 LED/m
constexpr uint16_t NB_LEDS_MAX = 300;               // taille du tampon en RAM
constexpr float    PAS_LED_MM  = 1000.0f / 144.0f;  // 6,944 mm
constexpr uint32_t LIMITE_MA   = 2500;              // limite de courant FastLED (alim 5 V 3 A)

// Clavier MX49
constexpr uint8_t NB_NOTES = 49;
constexpr uint8_t CANAL_BATTERIE = 10;  // canal MIDI de la batterie (GM)

// Bornes des réglages
constexpr float   OFFSET_MIN = -200.0f, OFFSET_MAX = 2100.0f;
constexpr float   PAS_MIN    = 5.0f,    PAS_MAX    = 30.0f;
constexpr uint16_t ANTICIPATION_MAX_MS = 2000;
}  // namespace cfg

// Que faire des notes du fichier hors de la plage du clavier
enum HorsPlage : uint8_t { HORS_PLAGE_IGNORER = 0, HORS_PLAGE_TRANSPOSER = 1 };

// Réglages modifiables sans recompiler, sauvegardés en NVS
struct Reglages {
    // Calibration (phase 1)
    float    offset_mm;      // position du centre de la 1re note, mesurée depuis le centre de la LED 0
    float    pas_note_mm;    // distance entre deux demi-tons (160 / 12 = 13,333)
    uint16_t nb_leds;        // nombre de LED réellement présentes
    bool     sens_inverse;   // true si l'entrée du bandeau est du côté droit du clavier
    uint8_t  note_basse;     // note MIDI de la touche la plus à gauche
    uint8_t  luminosite;     // 0..255
    uint8_t  leds_par_note;  // 1 ou 2
    // Lecture (phase 2)
    uint32_t couleur_md;       // main droite, 0xRRGGBB
    uint32_t couleur_mg;       // main gauche, 0xRRGGBB
    uint16_t anticipation_ms;  // 0 = pas d'anticipation
    uint8_t  hors_plage;       // HorsPlage
    bool     midi_out;         // envoyer les notes au MX49
};

extern Reglages reglages;

void reglagesParDefaut(Reglages& r);
void chargerReglages();
void sauverReglages();
void afficherReglages();

// Nom français d'une note MIDI, ex. 61 → "do#4" (convention : 60 = do4)
const char* nomNote(uint8_t note, char* tampon, size_t taille);
