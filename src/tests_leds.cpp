// tests_leds.cpp — animations de diagnostic non bloquantes
#include "tests_leds.h"
#include "bandeau.h"
#include "config.h"
#include "midi_out.h"

namespace tests {

enum class Anim : uint8_t { Aucune, Couleurs, Leds, Notes, MidiOut };

static Anim anim = Anim::Aucune;
static int etape = 0;
static uint32_t debutEtape = 0;
static uint32_t dureeEtape = 0;

static const uint8_t GAMME[] = {60, 62, 64, 65, 67, 69, 71, 72};
constexpr int NB_GAMME = sizeof GAMME;
static int noteMidiOutEnCours = -1;

// Touche noire ? L'index 0 est toujours un do sur le MX49 (touche physique la plus à gauche)
static bool toucheNoire(int indexNote) {
    switch (indexNote % 12) {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

static void terminer(const char* message) {
    anim = Anim::Aucune;
    bandeau::effacerFond();
    Serial.println(message);
}

static void etapeCouleurs() {
    static const char* const NOMS[] = {"ROUGE", "VERT", "BLEU"};
    static const CRGB COULEURS[] = {CRGB::Red, CRGB::Green, CRGB::Blue};
    if (etape >= 3) {
        terminer("Fin du test des couleurs. Si une couleur ne correspondait pas, l'ordre n'est pas GRB.");
        return;
    }
    bandeau::fondTout(COULEURS[etape]);
    Serial.printf("  Le bandeau doit être %s\n", NOMS[etape]);
}

static void etapeLeds() {
    const int n = reglages.nb_leds;
    if (etape > 0) {
        // La LED précédente s'éteint, sauf les dizaines qui restent en rouge pâle comme repères
        const int prec = etape - 1;
        bandeau::fondLed(prec, (prec % 10 == 0) ? CRGB(60, 0, 0) : CRGB::Black);
    }
    if (etape >= n) {
        terminer("Fin du chenillard des LED.");
        return;
    }
    bandeau::fondLed(etape, CRGB::White);
    Serial.printf("  LED %d\n", etape);
}

static void etapeNotes() {
    if (etape >= cfg::NB_NOTES) {
        terminer("Fin du chenillard des notes.");
        return;
    }
    bandeau::effacerFond();
    const bandeau::LedsNote l = bandeau::ledsDeNote(etape);
    bandeau::fondLeds(l, toucheNoire(etape) ? CRGB::Blue : CRGB::White);
    const uint8_t note = reglages.note_basse + etape;
    char nom[12];
    if (l.a < 0 && l.b < 0) {
        Serial.printf("  touche %2d  MIDI %3u  %-5s  -> hors bandeau !\n", etape, note, nomNote(note, nom, sizeof nom));
    } else if (l.b >= 0) {
        Serial.printf("  touche %2d  MIDI %3u  %-5s  LED %d + %d\n", etape, note, nomNote(note, nom, sizeof nom), l.a, l.b);
    } else {
        Serial.printf("  touche %2d  MIDI %3u  %-5s  LED %d\n", etape, note, nomNote(note, nom, sizeof nom), l.a);
    }
}

static void etapeMidiOut() {
    // Étapes paires : Note On (400 ms) ; étapes impaires : Note Off (100 ms)
    const int i = etape / 2;
    if (i >= NB_GAMME) {
        terminer("Fin de la gamme MIDI OUT.");
        return;
    }
    const uint8_t note = GAMME[i];
    const int index = (int)note - reglages.note_basse;
    char nom[12];
    if (etape % 2 == 0) {
        midiout::noteOn(note, 100, 1);
        noteMidiOutEnCours = note;
        bandeau::fondLeds(bandeau::ledsDeNote(index), CRGB::Blue);
        Serial.printf("  MIDI OUT : Note On  %3u %s (canal 1, vélocité 100)\n", note, nomNote(note, nom, sizeof nom));
        dureeEtape = 400;
    } else {
        midiout::noteOff(note, 1);
        noteMidiOutEnCours = -1;
        bandeau::fondLeds(bandeau::ledsDeNote(index), CRGB::Black);
        dureeEtape = 100;
    }
}

static void executerEtape() {
    switch (anim) {
        case Anim::Couleurs: etapeCouleurs(); break;
        case Anim::Leds:     etapeLeds(); break;
        case Anim::Notes:    etapeNotes(); break;
        case Anim::MidiOut:  etapeMidiOut(); break;
        default: break;
    }
}

static void demarrer(Anim a, uint32_t duree) {
    arreter();
    anim = a;
    etape = 0;
    dureeEtape = duree;
    debutEtape = millis();
    executerEtape();
}

void couleurs() {
    Serial.println("Test des couleurs (1 s chacune) :");
    demarrer(Anim::Couleurs, 1000);
}

void chenillardLeds(uint16_t ms) {
    Serial.printf("Chenillard des LED 0 à %u (%u ms par LED). Les dizaines restent en rouge pâle.\n",
                  reglages.nb_leds - 1, ms);
    demarrer(Anim::Leds, ms);
}

void chenillardNotes(uint16_t ms) {
    Serial.printf("Chenillard des %u notes (%u ms par note). Blanc = touche blanche, bleu = touche noire.\n",
                  cfg::NB_NOTES, ms);
    demarrer(Anim::Notes, ms);
}

void gammeMidiOut() {
    Serial.println("Envoi d'une gamme de do4 à do5 sur MIDI OUT (le MX49 doit la jouer) :");
    demarrer(Anim::MidiOut, 400);
}

void apercu() {
    arreter();
    for (int i = 0; i < cfg::NB_NOTES; i++) {
        CRGB c = toucheNoire(i) ? CRGB::Blue : CRGB(90, 90, 90);
        if (i % 12 == 0) c = CRGB::Red;  // les do en rouge comme repères
        bandeau::fondLeds(bandeau::ledsDeNote(i), c);
    }
    const bandeau::LedsNote premiere = bandeau::ledsDeNote(0);
    const bandeau::LedsNote derniere = bandeau::ledsDeNote(cfg::NB_NOTES - 1);
    Serial.printf("Aperçu : do en rouge, touches blanches en blanc, noires en bleu. 1re note -> LED %d, dernière -> LED %d.\n",
                  premiere.a, derniere.a);
    if (premiere.a < 0 || derniere.a < 0) Serial.println("ATTENTION : des notes tombent hors du bandeau !");
    Serial.println("('stop' pour éteindre)");
}

void noteSeule(int indexNote) {
    arreter();
    const bandeau::LedsNote l = bandeau::ledsDeNote(indexNote);
    bandeau::fondLeds(l, CRGB::White);
    const uint8_t note = reglages.note_basse + indexNote;
    char nom[12];
    Serial.printf("Touche %d (MIDI %u, %s) : position %.2f mm -> LED %d", indexNote, note,
                  nomNote(note, nom, sizeof nom), bandeau::positionNote(indexNote), l.a);
    if (l.b >= 0) Serial.printf(" + %d", l.b);
    Serial.println(l.a < 0 && l.b < 0 ? "  (hors bandeau !)" : "");
}

void arreter() {
    if (noteMidiOutEnCours >= 0) {
        midiout::noteOff(noteMidiOutEnCours, 1);
        noteMidiOutEnCours = -1;
    }
    anim = Anim::Aucune;
    bandeau::effacerFond();
}

bool enCours() { return anim != Anim::Aucune; }

void boucle() {
    if (anim == Anim::Aucune) return;
    const uint32_t t = millis();
    if (t - debutEtape < dureeEtape) return;
    debutEtape = t;
    etape++;
    executerEtape();
}

}  // namespace tests
