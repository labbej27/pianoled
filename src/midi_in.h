// midi_in.h — réception MIDI sur UART2 (MIDI IN du shield, relié au MIDI OUT du MX49)
#pragma once
#include <Arduino.h>

namespace midiin {

void maintenirTxAuRepos();  // TX2 au niveau haut dès le démarrage
void demarrer();
void boucle();  // à appeler à chaque tour de loop()

// Écoute : allumage des LED et affichage des notes reçues (active par défaut)
void activerEcoute(bool actif);
bool ecouteActive();
// Affichage des notes reçues dans le moniteur série
void activerTrace(bool actif);
bool traceActive();
// Trace brute : tous les messages reçus en hexadécimal (pour identifier des boutons)
void activerTraceBrute(bool actif);

// Mode « basse » : la prochaine Note On reçue devient note_basse
void attendreNoteBasse(bool actif);
bool attenteNoteBasse();

// État du clavier
bool enfoncee(uint8_t note);
uint32_t dernierAppui(uint8_t note);  // millis() du dernier appui

// Appelé à chaque appui (vélocité > 0) ou relâchement (vélocité 0)
using RappelNote = void (*)(uint8_t note, uint8_t velocite);
void definirRappel(RappelNote r);

// Boutons ▶ et ■ du MX49 (Note On 94 / 93 à vélocité 127 en DAW REMOTE, ou Start / Stop)
enum class Transport : uint8_t { Lecture, Stop };
using RappelTransport = void (*)(Transport t);
void definirRappelTransport(RappelTransport r);

}  // namespace midiin
