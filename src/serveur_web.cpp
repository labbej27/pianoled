// serveur_web.cpp — serveur HTTP + WebSocket (ESPAsyncWebServer)
#include "serveur_web.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "commandes_serie.h"
#include "bandeau.h"
#include "config.h"
#include "midi_in.h"
#include "moteur_lecture.h"
#include "page_web.h"
#include "reseau.h"
#include "stockage.h"
#include "tests_leds.h"

namespace serveurweb {

static AsyncWebServer serveur(80);
static AsyncWebSocket ws("/ws");

struct Message {
    char texte[200];
};
static QueueHandle_t fileCommandes = nullptr;

static volatile bool nouveauClient = false;
static volatile bool fichiersModifies = false;
static bool etatForce = false;
static uint32_t dernierEtat = 0, dernierNettoyage = 0, derniereVerif = 0;
static Reglages reglagesEnvoyes;
static String erreurEnvoi;

// ---------- Côté AsyncTCP (cœur 0) : rien de long ----------

static void surEvenementWs(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType type, void* arg, uint8_t* data,
                           size_t len) {
    if (type == WS_EVT_CONNECT) {
        nouveauClient = true;
    } else if (type == WS_EVT_DATA) {
        const AwsFrameInfo* info = (const AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT &&
            len < sizeof(Message::texte)) {
            Message m;
            memcpy(m.texte, data, len);
            m.texte[len] = '\0';
            xQueueSend(fileCommandes, &m, 0);  // file pleine : la commande est perdue, pas de blocage
        }
    }
}

static const char* nomDeBase(const String& nom) {
    const char* p = nom.c_str();
    const char* s = strrchr(p, '/');
    const char* b = strrchr(p, '\\');
    if (b > s) s = b;
    return s ? s + 1 : p;
}

static void routes() {
    serveur.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        AsyncWebServerResponse* rep =
            r->beginResponse(200, "text/html; charset=utf-8", (const uint8_t*)PAGE_WEB, strlen(PAGE_WEB));
        rep->addHeader("Cache-Control", "no-cache");
        r->send(rep);
    });

    serveur.on("/api/fichiers", HTTP_GET, [](AsyncWebServerRequest* r) {
        String s;
        stockage::listerJson(s);
        r->send(200, "application/json", s);
    });

    serveur.on("/api/supprimer", HTTP_POST, [](AsyncWebServerRequest* r) {
        if (!r->hasParam("f")) {
            r->send(400, "text/plain; charset=utf-8", "paramètre f manquant");
            return;
        }
        const bool ok = stockage::supprimer(r->getParam("f")->value().c_str());
        fichiersModifies = true;
        r->send(ok ? 200 : 404, "text/plain; charset=utf-8", ok ? "supprimé" : "fichier introuvable");
    });

    // Envoi d'un fichier : écrit par morceaux dans LittleFS au fil de la réception
    serveur.on(
        "/api/envoi", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            if (erreurEnvoi.length()) {
                r->send(400, "text/plain; charset=utf-8", erreurEnvoi);
            } else {
                fichiersModifies = true;
                r->send(200, "text/plain; charset=utf-8", "ok");
            }
        },
        [](AsyncWebServerRequest*, const String& nom, size_t index, uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                erreurEnvoi = "";
                if (!stockage::debutEnvoi(nomDeBase(nom), erreurEnvoi)) return;
            }
            if (erreurEnvoi.length()) return;
            if (len && !stockage::ecrireEnvoi(data, len, erreurEnvoi)) return;
            if (final) stockage::finEnvoi(erreurEnvoi);
        });

    serveur.onNotFound([](AsyncWebServerRequest* r) { r->send(404, "text/plain; charset=utf-8", "introuvable"); });
}

// ---------- Côté loop() (cœur 1) ----------

static void envoyerTexte(const String& s) {
    if (ws.count()) ws.textAll(s);
}

static void envoyerMessage(const String& texte, bool ok) {
    JsonDocument d;
    d["t"] = "msg";
    d["ok"] = ok;
    d["texte"] = texte;
    String s;
    serializeJson(d, s);
    envoyerTexte(s);
}

static void envoyerReglages() {
    char hex[8];
    JsonDocument d;
    d["t"] = "reglages";
    d["offset"] = roundf(reglages.offset_mm * 100) / 100;
    d["pas"] = roundf(reglages.pas_note_mm * 1000) / 1000;
    d["inverse"] = reglages.sens_inverse ? 1 : 0;
    d["lum"] = reglages.luminosite;
    d["nbleds"] = reglages.nb_leds;
    d["lpn"] = reglages.leds_par_note;
    d["basse"] = reglages.note_basse;
    d["antic"] = reglages.anticipation_ms;
    d["horsplage"] = reglages.hors_plage;
    d["mout"] = reglages.midi_out ? 1 : 0;
    snprintf(hex, sizeof hex, "#%06x", (unsigned)reglages.couleur_md);
    d["cmd"] = hex;
    snprintf(hex, sizeof hex, "#%06x", (unsigned)reglages.couleur_mg);
    d["cmg"] = hex;
    d["wifi"] = reseau::description();
    String s;
    serializeJson(d, s);
    envoyerTexte(s);
    memcpy(&reglagesEnvoyes, &reglages, sizeof reglages);  // copie octet à octet (memcmp plus bas)
}

static void envoyerMorceau() {
    if (moteur::etat() == moteur::Etat::Vide) return;
    String s;
    moteur::morceauJson(s);
    envoyerTexte(s);
}

static void executer(const char* texte) {
    JsonDocument d;
    if (deserializeJson(d, texte)) return;
    const char* c = d["c"] | "";
    String msg;
    bool ok = true;

    if (!strcmp(c, "charger")) {
        tests::arreter();
        const char* f = d["f"] | "";
        ok = moteur::charger(f, msg);
        if (ok) {
            msg = String("« ") + f + " » chargé";
            envoyerMorceau();
        } else {
            msg = String("Impossible de charger « ") + f + " » : " + msg;
        }
    } else if (!strcmp(c, "lecture")) {
        tests::arreter();
        moteur::lecture();
    } else if (!strcmp(c, "pause")) {
        moteur::pause();
    } else if (!strcmp(c, "stop")) {
        moteur::stop();
    } else if (!strcmp(c, "vitesse")) {
        moteur::vitesse(d["v"] | 100);
    } else if (!strcmp(c, "mode")) {
        moteur::mode(strcmp(d["v"] | "demo", "attente") == 0 ? moteur::Mode::Attente : moteur::Mode::Demo);
    } else if (!strcmp(c, "travail")) {
        moteur::travail(d["v"] | 0);
    } else if (!strcmp(c, "aller")) {
        moteur::allerA(d["v"] | 0);
    } else if (!strcmp(c, "a")) {
        moteur::pointA();
    } else if (!strcmp(c, "b")) {
        moteur::pointB();
    } else if (!strcmp(c, "ab_effacer")) {
        moteur::effacerAB();
    } else if (!strcmp(c, "boucle")) {
        moteur::boucleAB(d["v"] | false);
    } else if (!strcmp(c, "main")) {
        moteur::attribuerMain(d["g"] | 255, d["v"] | 0);
        envoyerMorceau();
    } else if (!strcmp(c, "partage")) {
        moteur::partage(d["v"] | 60);
        envoyerMorceau();
    } else if (!strcmp(c, "reglage")) {
        ok = commandes::modifierReglage(d["k"] | "", d["v"] | 0.0f, msg);
        envoyerReglages();
    } else if (!strcmp(c, "couleur")) {
        ok = commandes::modifierCouleur(d["main"] | "", d["v"] | "", msg);
        envoyerReglages();
    } else if (!strcmp(c, "octave")) {
        // Même sens que les boutons OCTAVE du MX49 : + monte le clavier d'une octave
        const int nouvelle = reglages.note_basse + 12 * ((d["v"] | 0) > 0 ? 1 : -1);
        if (nouvelle < 0 || nouvelle > 127 - (cfg::NB_NOTES - 1)) {
            ok = false;
            msg = "Octave hors limites";
        } else {
            reglages.note_basse = nouvelle;
            sauverReglages();
            bandeau::toutesNotesJoueesEteintes();
            moteur::reglagesModifies();
            char a[12], b[12];
            msg = String("Clavier : ") + nomNote(nouvelle, a, sizeof a) + " à " +
                  nomNote(nouvelle + cfg::NB_NOTES - 1, b, sizeof b);
        }
        envoyerReglages();
    } else if (!strcmp(c, "basse")) {
        midiin::attendreNoteBasse(true);
        msg = "Appuie sur la touche la plus à gauche du MX49";
    } else if (!strcmp(c, "apercu")) {
        tests::apercu();
    } else if (!strcmp(c, "eteindre")) {
        tests::arreter();
    }
    etatForce = true;
    if (msg.length()) envoyerMessage(msg, ok);
}

void demarrer() {
    fileCommandes = xQueueCreate(16, sizeof(Message));
    ws.onEvent(surEvenementWs);
    serveur.addHandler(&ws);
    routes();
    serveur.begin();
    memcpy(&reglagesEnvoyes, &reglages, sizeof reglages);  // copie octet à octet (memcmp plus bas)
    Serial.println("Serveur web démarré (port 80).");
}

void boucle() {
    Message m;
    for (int n = 0; n < 8 && xQueueReceive(fileCommandes, &m, 0) == pdTRUE; n++) executer(m.texte);

    const uint32_t t = millis();
    if (nouveauClient) {
        nouveauClient = false;
        envoyerReglages();
        envoyerMorceau();
        etatForce = true;
    }
    if (fichiersModifies) {
        fichiersModifies = false;
        envoyerTexte("{\"t\":\"fichiers\"}");
    }
    // Réglages modifiés ailleurs (moniteur série, capture de la note basse…)
    if (t - derniereVerif >= 500) {
        derniereVerif = t;
        if (memcmp(&reglagesEnvoyes, &reglages, sizeof reglages) != 0) envoyerReglages();
    }
    // État : 10 fois par seconde pendant la lecture, une fois par seconde sinon
    const moteur::Etat e = moteur::etat();
    const uint32_t periode = (e == moteur::Etat::Lecture || e == moteur::Etat::AttenteJoueur) ? 100 : 1000;
    if (ws.count() && (etatForce || t - dernierEtat >= periode)) {
        etatForce = false;
        dernierEtat = t;
        String s;
        moteur::etatJson(s);
        envoyerTexte(s);
    }
    if (t - dernierNettoyage >= 1000) {
        dernierNettoyage = t;
        ws.cleanupClients();
    }
}

}  // namespace serveurweb
