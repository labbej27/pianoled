// main.cpp — bandeau LED d'apprentissage du piano (Yamaha MX49 + ESP32)
//
// Répartition sur les deux cœurs :
//   cœur 0 : Wi-Fi, AsyncTCP, serveur web (callbacks courts, commandes mises en file)
//   cœur 1 : loop() — MIDI IN, moteur de lecture, LED, commandes série et web
#include <Arduino.h>
#include "bandeau.h"
#include "commandes_serie.h"
#include "config.h"
#include "midi_in.h"
#include "moteur_lecture.h"
#include "reseau.h"
#include "serveur_web.h"
#include "stockage.h"
#include "tests_leds.h"

void setup() {
    // 1. Avant tout : éteindre le bandeau, ligne MIDI OUT au repos
    bandeau::eteindreImmediatement();
    midiin::maintenirTxAuRepos();

    // 2. Moniteur série (USB, UART0) ; tampon d'émission pour ne pas bloquer la boucle
    Serial.setTxBufferSize(2048);
    Serial.begin(cfg::VITESSE_SERIE);
    Serial.println();
    Serial.println("Bandeau LED piano — démarrage");

    // 3. Réglages persistants, puis application au bandeau
    chargerReglages();
    bandeau::appliquerReglages();

    // 4. MIDI sur UART2 (RX 16, TX 17), moteur de lecture
    midiin::demarrer();
    moteur::demarrer();
    Serial.println("Écoute MIDI active sur RX2 (GPIO 16), 31 250 bauds.");

    // 5. Fichiers, réseau, interface web
    stockage::demarrer();
    reseau::demarrer();
    serveurweb::demarrer();

    afficherReglages();
    commandes::afficherAide();
}

void loop() {
    // Tout est non bloquant : chaque module fait un petit pas puis rend la main
    midiin::boucle();
    moteur::boucle();
    commandes::boucle();
    serveurweb::boucle();
    reseau::boucle();
    tests::boucle();
    bandeau::miseAJour();
}
