// reseau.cpp — connexion Wi-Fi non bloquante, bascule en point d'accès, mDNS pianoled.local
#include "reseau.h"
#include <ESPmDNS.h>
#include <WiFi.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

namespace reseau {

static const char* NOM_HOTE = "pianoled";
static const char* NOM_AP = "PianoLED";
constexpr uint32_t DELAI_CONNEXION_MS = 15000;

enum class Etat : uint8_t { Connexion, Connecte, PointAcces };
static Etat etat = Etat::Connexion;
static uint32_t debut = 0;
static bool mdnsActif = false;

static void demarrerMdns() {
    if (mdnsActif) MDNS.end();
    mdnsActif = MDNS.begin(NOM_HOTE);
    if (mdnsActif) MDNS.addService("http", "tcp", 80);
}

static void basculerPointAcces() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(NOM_AP, AP_MOT_DE_PASSE);
    etat = Etat::PointAcces;
    demarrerMdns();
    Serial.printf("Wi-Fi : point d'accès « %s » (mot de passe dans secrets.h), http://%s\n", NOM_AP,
                  WiFi.softAPIP().toString().c_str());
}

void demarrer() {
    WiFi.persistent(false);
    WiFi.setHostname(NOM_HOTE);
    if (strlen(WIFI_SSID) == 0) {
        Serial.println("Wi-Fi : aucun réseau configuré (secrets.h).");
        basculerPointAcces();
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // latence plus faible pour le WebSocket
    WiFi.begin(WIFI_SSID, WIFI_MOT_DE_PASSE);
    etat = Etat::Connexion;
    debut = millis();
    Serial.printf("Wi-Fi : connexion à « %s »...\n", WIFI_SSID);
}

void boucle() {
    switch (etat) {
        case Etat::Connexion:
            if (WiFi.status() == WL_CONNECTED) {
                etat = Etat::Connecte;
                WiFi.setAutoReconnect(true);
                demarrerMdns();
                Serial.printf("Wi-Fi : connecté, http://%s.local (%s)\n", NOM_HOTE,
                              WiFi.localIP().toString().c_str());
            } else if (millis() - debut > DELAI_CONNEXION_MS) {
                Serial.println("Wi-Fi : échec de la connexion, bascule en point d'accès.");
                basculerPointAcces();
            }
            break;
        case Etat::Connecte:
            // La reconnexion automatique du cœur Wi-Fi gère les coupures passagères
            break;
        case Etat::PointAcces:
            break;
    }
}

String description() {
    switch (etat) {
        case Etat::Connexion: return String("connexion à « ") + WIFI_SSID + " » en cours";
        case Etat::Connecte:
            return String(WiFi.status() == WL_CONNECTED ? "connecté à « " : "reconnexion à « ") + WIFI_SSID +
                   " », http://pianoled.local (" + WiFi.localIP().toString() + ")";
        case Etat::PointAcces:
            return String("point d'accès « ") + NOM_AP + " », http://" + WiFi.softAPIP().toString();
    }
    return "";
}

}  // namespace reseau
