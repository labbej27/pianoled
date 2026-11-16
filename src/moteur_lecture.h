// moteur_lecture.h — lecture d'un morceau MIDI sur le bandeau (modes démo et attente)
//
// Tourne dans loop() (cœur 1). Le temps du morceau avance de dt × vitesse à chaque tour :
// un changement de vitesse s'applique à partir de l'instant présent, sans saut ni dérive.
#pragma once
#include <Arduino.h>

namespace moteur {

enum class Etat : uint8_t { Vide, Arret, Lecture, Pause, AttenteJoueur };
enum class Mode : uint8_t { Demo, Attente };

// Attribution d'un groupe (piste + canal) à une main
enum Main : uint8_t { MAIN_MASQUEE = 0, MAIN_DROITE = 1, MAIN_GAUCHE = 2, MAIN_HAUTEUR = 3 };
// Main(s) travaillée(s) en mode attente
enum Travail : uint8_t { TRAVAIL_DEUX = 0, TRAVAIL_DROITE = 1, TRAVAIL_GAUCHE = 2 };

constexpr uint8_t VITESSE_MIN = 25, VITESSE_MAX = 150;

void demarrer();
void boucle();

// Chargement depuis LittleFS ; renvoie false avec un message en cas d'échec
bool charger(const char* nom, String& erreur);

void lecture();
void pause();
void stop();
void allerA(uint32_t ms);
void vitesse(uint8_t pourcent);
void mode(Mode m);
void travail(uint8_t t);
void attribuerMain(uint8_t groupe, uint8_t main);
void partage(uint8_t note);  // séparation main gauche / droite pour l'attribution « par hauteur »
void pointA();
void pointB();
void effacerAB();
void boucleAB(bool actif);
void reglagesModifies();  // calibration, couleurs, transposition : recalcule l'affichage

Etat etat();
const char* nomEtat();

// JSON pour l'interface web
void etatJson(String& sortie);      // position, tempo, notes en cours… (fréquent)
void morceauJson(String& sortie);   // groupes et mains (au chargement / changement)

}  // namespace moteur
