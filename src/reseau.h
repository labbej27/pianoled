// reseau.h — Wi-Fi (réseau de la maison, ou point d'accès PianoLED en secours) et mDNS
#pragma once
#include <Arduino.h>

namespace reseau {

void demarrer();  // non bloquant : la connexion se poursuit dans boucle()
void boucle();
String description();  // ex. « connecté à Maison, http://pianoled.local (192.168.1.42) »

}  // namespace reseau
