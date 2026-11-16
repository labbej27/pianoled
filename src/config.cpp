// config.cpp — chargement / sauvegarde des réglages en NVS (bibliothèque Preferences)
#include "config.h"
#include <Preferences.h>

Reglages reglages;

static Preferences prefs;
static const char* ESPACE_NVS = "pianoled";

void reglagesParDefaut(Reglages& r) {
    r.offset_mm     = 0.0f;
    r.pas_note_mm   = 160.0f / 12.0f;
    r.nb_leds       = 144;
    r.sens_inverse  = false;
    r.note_basse    = 36;
    r.luminosite    = 60;
    r.leds_par_note = 1;
    r.couleur_md      = 0x0050FF;  // bleu
    r.couleur_mg      = 0xFF7800;  // orange (le vert est réservé au retour « bonne note »)
    r.anticipation_ms = 500;
    r.hors_plage      = HORS_PLAGE_TRANSPOSER;
    r.midi_out        = false;
}

// Ramène chaque réglage dans une plage valide (protection contre une NVS corrompue)
static void borner(Reglages& r) {
    Reglages d;
    reglagesParDefaut(d);
    if (!(r.offset_mm >= cfg::OFFSET_MIN && r.offset_mm <= cfg::OFFSET_MAX)) r.offset_mm = d.offset_mm;
    if (!(r.pas_note_mm >= cfg::PAS_MIN && r.pas_note_mm <= cfg::PAS_MAX)) r.pas_note_mm = d.pas_note_mm;
    if (r.nb_leds < 1 || r.nb_leds > cfg::NB_LEDS_MAX) r.nb_leds = d.nb_leds;
    if (r.note_basse > 127 - (cfg::NB_NOTES - 1)) r.note_basse = d.note_basse;
    if (r.leds_par_note != 1 && r.leds_par_note != 2) r.leds_par_note = d.leds_par_note;
    r.couleur_md &= 0xFFFFFF;
    r.couleur_mg &= 0xFFFFFF;
    if (r.anticipation_ms > cfg::ANTICIPATION_MAX_MS) r.anticipation_ms = d.anticipation_ms;
    if (r.hors_plage > HORS_PLAGE_TRANSPOSER) r.hors_plage = d.hors_plage;
}

void chargerReglages() {
    reglagesParDefaut(reglages);
    // Lecture seule : échoue proprement si l'espace n'existe pas encore (premier démarrage)
    if (prefs.begin(ESPACE_NVS, true)) {
        reglages.offset_mm     = prefs.getFloat("offset", reglages.offset_mm);
        reglages.pas_note_mm   = prefs.getFloat("pas", reglages.pas_note_mm);
        reglages.nb_leds       = prefs.getUShort("nbleds", reglages.nb_leds);
        reglages.sens_inverse  = prefs.getBool("inverse", reglages.sens_inverse);
        reglages.note_basse    = prefs.getUChar("basse", reglages.note_basse);
        reglages.luminosite    = prefs.getUChar("lum", reglages.luminosite);
        reglages.leds_par_note = prefs.getUChar("lpn", reglages.leds_par_note);
        reglages.couleur_md      = prefs.getULong("cmd", reglages.couleur_md);
        reglages.couleur_mg      = prefs.getULong("cmg", reglages.couleur_mg);
        reglages.anticipation_ms = prefs.getUShort("antic", reglages.anticipation_ms);
        reglages.hors_plage      = prefs.getUChar("horsplage", reglages.hors_plage);
        reglages.midi_out        = prefs.getBool("mout", reglages.midi_out);
        prefs.end();
        Serial.println("Réglages chargés depuis la NVS.");
    } else {
        Serial.println("Aucun réglage en NVS : valeurs par défaut.");
    }
    borner(reglages);
}

void sauverReglages() {
    borner(reglages);
    if (!prefs.begin(ESPACE_NVS, false)) {
        Serial.println("ERREUR : impossible d'ouvrir la NVS en écriture.");
        return;
    }
    prefs.putFloat("offset", reglages.offset_mm);
    prefs.putFloat("pas", reglages.pas_note_mm);
    prefs.putUShort("nbleds", reglages.nb_leds);
    prefs.putBool("inverse", reglages.sens_inverse);
    prefs.putUChar("basse", reglages.note_basse);
    prefs.putUChar("lum", reglages.luminosite);
    prefs.putUChar("lpn", reglages.leds_par_note);
    prefs.putULong("cmd", reglages.couleur_md);
    prefs.putULong("cmg", reglages.couleur_mg);
    prefs.putUShort("antic", reglages.anticipation_ms);
    prefs.putUChar("horsplage", reglages.hors_plage);
    prefs.putBool("mout", reglages.midi_out);
    prefs.end();
}

void afficherReglages() {
    char nom[12];
    Serial.println("--- Réglages actuels ---");
    Serial.printf("  offset_mm     = %.2f\n", reglages.offset_mm);
    Serial.printf("  pas_note_mm   = %.3f\n", reglages.pas_note_mm);
    Serial.printf("  nb_leds       = %u\n", reglages.nb_leds);
    Serial.printf("  sens_inverse  = %u\n", reglages.sens_inverse ? 1 : 0);
    Serial.printf("  note_basse    = %u (%s)\n", reglages.note_basse,
                  nomNote(reglages.note_basse, nom, sizeof nom));
    Serial.printf("  luminosite    = %u / 255\n", reglages.luminosite);
    Serial.printf("  leds_par_note = %u\n", reglages.leds_par_note);
    Serial.printf("  couleur_md    = #%06X   couleur_mg = #%06X\n", reglages.couleur_md, reglages.couleur_mg);
    Serial.printf("  anticipation  = %u ms\n", reglages.anticipation_ms);
    Serial.printf("  hors plage    = %s\n", reglages.hors_plage == HORS_PLAGE_TRANSPOSER ? "transposer" : "ignorer");
    Serial.printf("  midi_out      = %u\n", reglages.midi_out ? 1 : 0);
    Serial.printf("  (pas d'une LED = %.3f mm, fixe)\n", cfg::PAS_LED_MM);
}

const char* nomNote(uint8_t note, char* tampon, size_t taille) {
    static const char* const NOMS[12] = {"do", "do#", "ré", "ré#", "mi", "fa",
                                         "fa#", "sol", "sol#", "la", "la#", "si"};
    snprintf(tampon, taille, "%s%d", NOMS[note % 12], note / 12 - 1);
    return tampon;
}
