// commandes_serie.h — interpréteur de commandes texte du moniteur série (maintenance)
#pragma once
#include <Arduino.h>

namespace commandes {

void afficherAide();
void boucle();  // à appeler à chaque tour de loop() : lit le port série sans bloquer

// Modification d'un réglage (partagée avec l'interface web) : contrôle, sauvegarde NVS, application.
// cle : offset, pas, inverse, lum, nbleds, ledsparnote, basse, antic, horsplage, mout
bool modifierReglage(const char* cle, float valeur, String& message);
bool modifierCouleur(const char* main, const char* hexRRGGBB, String& message);  // main : md ou mg

}  // namespace commandes
