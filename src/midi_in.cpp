// midi_in.cpp — réception MIDI (bibliothèque MIDI de FortySevenEffects sur Serial2)
#include "midi_in.h"
#include "bandeau.h"
#include "config.h"
#include "midi_interface.h"

MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);

namespace midiin {

static bool ecoute = true;
static bool trace = true;
static bool traceBrute = false;
static bool attenteBasse = false;
static bool touches[128];
static uint32_t appuis[128];
static RappelNote rappel = nullptr;
static RappelTransport rappelTransport = nullptr;
static bool boutonTenu[2];  // ■ (93) et ▶ (94) du mode DAW REMOTE, pour avaler leur Note Off

// En mode DAW REMOTE, le MX49 envoie ses boutons de transport comme des notes (protocole
// Mackie Control) : Note On 93 = ■, 94 = ▶, toujours à vélocité 127 sur le canal 1.
static bool estBoutonDaw(byte canal, byte note) { return canal == 1 && (note == 93 || note == 94); }

static void surNoteOff(byte canal, byte note, byte) {
    if (estBoutonDaw(canal, note) && boutonTenu[note - 93]) {
        boutonTenu[note - 93] = false;
        return;
    }
    if (canal == cfg::CANAL_BATTERIE) return;  // rythmique du MX49 : pas de LED
    touches[note & 0x7F] = false;
    if (attenteBasse || !ecoute) return;
    bandeau::noteJouee((int)note - reglages.note_basse, 0);
    if (rappel) rappel(note, 0);
    if (trace) {
        char nom[12];
        Serial.printf("MIDI  Note Off  canal %2u  note %3u  %s\n", canal, note, nomNote(note, nom, sizeof nom));
    }
}

static void surNoteOn(byte canal, byte note, byte velocite) {
    if (velocite == 0) {  // Note On de vélocité 0 = Note Off
        surNoteOff(canal, note, 0);
        return;
    }
    if (estBoutonDaw(canal, note) && velocite == 127 && rappelTransport) {
        boutonTenu[note - 93] = true;
        rappelTransport(note == 94 ? Transport::Lecture : Transport::Stop);
        return;
    }
    if (canal == cfg::CANAL_BATTERIE) return;
    note &= 0x7F;
    touches[note] = true;
    appuis[note] = millis();
    char nom[12];
    nomNote(note, nom, sizeof nom);

    if (attenteBasse) {
        if (note > 127 - (cfg::NB_NOTES - 1)) {
            Serial.printf("Note %u trop aiguë pour être la touche la plus à gauche, réessaie.\n", note);
            return;
        }
        attenteBasse = false;
        reglages.note_basse = note;
        sauverReglages();
        bandeau::toutesNotesJoueesEteintes();
        Serial.printf("OK : note_basse = %u (%s), sauvegardée. Le clavier couvre %u à %u.\n", note, nom, note,
                      note + cfg::NB_NOTES - 1);
        return;
    }
    if (!ecoute) return;

    const int i = (int)note - reglages.note_basse;
    bandeau::noteJouee(i, velocite);
    if (rappel) rappel(note, velocite);
    if (!trace) return;
    if (i < 0 || i >= cfg::NB_NOTES) {
        Serial.printf("MIDI  Note On   canal %2u  note %3u  %-5s vélocité %3u  -> hors clavier (%u à %u)\n", canal,
                      note, nom, velocite, reglages.note_basse, reglages.note_basse + cfg::NB_NOTES - 1);
        return;
    }
    const bandeau::LedsNote l = bandeau::ledsDeNote(i);
    if (l.a < 0 && l.b < 0) {
        Serial.printf("MIDI  Note On   canal %2u  note %3u  %-5s vélocité %3u  -> touche %2d, hors bandeau !\n",
                      canal, note, nom, velocite, i);
    } else {
        Serial.printf("MIDI  Note On   canal %2u  note %3u  %-5s vélocité %3u  -> touche %2d, LED %d\n", canal, note,
                      nom, velocite, i, l.a >= 0 ? l.a : l.b);
    }
}

static void surControle(byte, byte numero, byte) {
    // 120 = All Sound Off, 123 = All Notes Off
    if (numero == 120 || numero == 123) bandeau::toutesNotesJoueesEteintes();
}

// Trace brute : chaque message reçu, en hexadécimal (horloge et Active Sensing filtrés)
static void afficherBrut() {
    const uint8_t type = MIDI.getType();
    if (type == 0xF8 || type == 0xFE) return;
    if (type == 0xF0) {
        const uint8_t* d = MIDI.getSysExArray();
        const unsigned n = MIDI.getSysExArrayLength();
        Serial.printf("BRUT  SysEx (%u octets) :", n);
        for (unsigned j = 0; j < n && j < 48; j++) Serial.printf(" %02X", d[j]);
        Serial.println(n > 48 ? " ..." : "");
        return;
    }
    if (type >= 0xF0) {  // temps réel / commun : Start FA, Continue FB, Stop FC, Song Position F2...
        Serial.printf("BRUT  %02X  d1 %02X  d2 %02X\n", type, MIDI.getData1(), MIDI.getData2());
        return;
    }
    Serial.printf("BRUT  %02X (type %02X canal %2u)  d1 %3u (%02X)  d2 %3u (%02X)\n", type | (MIDI.getChannel() - 1),
                  type, MIDI.getChannel(), MIDI.getData1(), MIDI.getData1(), MIDI.getData2(), MIDI.getData2());
}

// Messages temps réel Start / Continue / Stop (boutons ▶ ■ hors DAW REMOTE)
static void surStart() { if (rappelTransport) rappelTransport(Transport::Lecture); }
static void surStop() { if (rappelTransport) rappelTransport(Transport::Stop); }

void maintenirTxAuRepos() {
    // Ligne MIDI au repos = niveau haut. À appeler au tout début de setup() pour que
    // TX2 ne flotte pas pendant les initialisations.
    pinMode(cfg::BROCHE_MIDI_TX, OUTPUT);
    digitalWrite(cfg::BROCHE_MIDI_TX, HIGH);
}

void demarrer() {
    MIDI.setHandleNoteOn(surNoteOn);
    MIDI.setHandleNoteOff(surNoteOff);
    MIDI.setHandleControlChange(surControle);
    MIDI.setHandleStart(surStart);
    MIDI.setHandleContinue(surStart);
    MIDI.setHandleStop(surStop);
    // Une seule ouverture de l'UART : MIDI.begin() appelle Serial2.begin(31250), et le cœur
    // Arduino 2.0.x place par défaut l'UART2 sur RX 16 / TX 17. Rouvrir l'UART ensuite
    // produirait une impulsion sur TX, que le MX49 signale par « MIDI data error ».
    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.turnThruOff();  // ne pas renvoyer ce qu'on reçoit vers MIDI OUT
}

void boucle() {
    // Traite tous les messages en attente, sans bloquer
    for (int k = 0; k < 64 && MIDI.read(); k++) {
        if (traceBrute) afficherBrut();
    }
}

void activerEcoute(bool actif) {
    ecoute = actif;
    if (!actif) bandeau::toutesNotesJoueesEteintes();
}
bool ecouteActive() { return ecoute; }
void activerTrace(bool actif) { trace = actif; }
void activerTraceBrute(bool actif) { traceBrute = actif; }
bool traceActive() { return trace; }

void attendreNoteBasse(bool actif) { attenteBasse = actif; }
bool attenteNoteBasse() { return attenteBasse; }

bool enfoncee(uint8_t note) { return touches[note & 0x7F]; }
uint32_t dernierAppui(uint8_t note) { return appuis[note & 0x7F]; }
void definirRappel(RappelNote r) { rappel = r; }
void definirRappelTransport(RappelTransport r) { rappelTransport = r; }

}  // namespace midiin
