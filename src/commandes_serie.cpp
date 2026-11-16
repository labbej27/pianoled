// commandes_serie.cpp — commandes texte (maintenance) : tests, calibration, réglages, lecture
#include "commandes_serie.h"
#include <Arduino.h>
#include "bandeau.h"
#include "config.h"
#include "midi_in.h"
#include "moteur_lecture.h"
#include "reseau.h"
#include "stockage.h"
#include "tests_leds.h"

namespace commandes {

static char ligne[96];
static uint8_t longueur = 0;

// --- Calibration guidée ---
static int etapeCalib = 0;  // 0 = inactive, 1 = note de gauche, 2 = note de droite
static float posCalib = 0.0f;
static float posGauche = 0.0f;

void afficherAide() {
    Serial.println();
    Serial.println("=== Bandeau LED piano — commandes de maintenance ===");
    Serial.println("Tests :");
    Serial.println("  couleurs          rouge, vert, bleu (1 s chacun) pour vérifier l'ordre GRB");
    Serial.println("  leds [ms]         allume les LED une par une (80 ms par défaut)");
    Serial.println("  notes [ms]        chenillard sur les 49 notes (300 ms par défaut)");
    Serial.println("  note N            allume uniquement la touche N (0 à 48)");
    Serial.println("  apercu            affiche les 49 notes (do rouges, noires bleues)");
    Serial.println("  stop              arrête le test et la lecture en cours");
    Serial.println("  midiout           envoie une gamme de do au MX49 par MIDI OUT");
    Serial.println("Réglages (sauvegardés en NVS) :");
    Serial.println("  offset X          position de la 1re note en mm depuis la LED 0");
    Serial.println("  pas X             distance entre deux demi-tons en mm (13.333)");
    Serial.println("  inverse 0|1       1 si le bandeau part du côté droit du clavier");
    Serial.println("  lum X             luminosité 0 à 255");
    Serial.println("  nbleds X          nombre de LED du bandeau (1 à 300)");
    Serial.println("  ledsparnote 1|2   une ou deux LED par note");
    Serial.println("  basse             puis appuie sur la touche la plus à gauche du MX49");
    Serial.println("  basse N           fixe note_basse à la main (36 = do2)");
    Serial.println("  antic X           anticipation en ms (0 = aucune)");
    Serial.println("  horsplage 0|1     notes hors clavier : 0 ignorer, 1 transposer");
    Serial.println("  mout 0|1          envoyer les notes du morceau au MX49");
    Serial.println("  calib             calibration guidée (note de gauche puis de droite)");
    Serial.println("  config            affiche les réglages");
    Serial.println("  defaut oui        remet tous les réglages par défaut");
    Serial.println("Lecture et réseau :");
    Serial.println("  fichiers          liste les fichiers MIDI");
    Serial.println("  jouer NOM.mid     charge et lance un morceau");
    Serial.println("  pause             pause / reprise");
    Serial.println("  etat              état de la lecture");
    Serial.println("  wifi              état du Wi-Fi et adresse de l'interface web");
    Serial.println("MIDI :");
    Serial.println("  ecoute 0|1        coupe / active l'écoute MIDI IN (active par défaut)");
    Serial.println("  trace 0|1|2       notes reçues : non / oui / tous les messages en hexa");
    Serial.println("  aide              affiche cette aide");
    Serial.println();
}

// Lit un nombre décimal ; refuse les chaînes vides ou suivies de caractères parasites
static bool lireNombre(const char* s, float& v) {
    if (!s || !*s) return false;
    char* fin = nullptr;
    v = strtof(s, &fin);
    while (fin && *fin == ' ') fin++;
    return fin && *fin == '\0' && fin != s;
}

static bool lireEntier(const char* s, long mini, long maxi, long& v) {
    float f;
    if (!lireNombre(s, f) || f != floorf(f) || f < mini || f > maxi) return false;
    v = (long)f;
    return true;
}

static bool lectureEnCours() {
    const moteur::Etat e = moteur::etat();
    return e == moteur::Etat::Lecture || e == moteur::Etat::AttenteJoueur || e == moteur::Etat::Pause;
}

// Après un changement de réglage : sauvegarde, application, aperçu si rien n'est en lecture
static void reglageModifie(bool apercu) {
    sauverReglages();
    bandeau::toutesNotesJoueesEteintes();
    bandeau::appliquerReglages();
    moteur::reglagesModifies();
    if (apercu && !lectureEnCours()) tests::apercu();
}

bool modifierReglage(const char* cle, float x, String& message) {
    char tampon[96];
    const bool entier = (x == floorf(x));
    if (!strcmp(cle, "offset")) {
        if (x < cfg::OFFSET_MIN || x > cfg::OFFSET_MAX) {
            snprintf(tampon, sizeof tampon, "offset : de %.0f à %.0f mm", cfg::OFFSET_MIN, cfg::OFFSET_MAX);
            message = tampon;
            return false;
        }
        reglages.offset_mm = x;
        snprintf(tampon, sizeof tampon, "offset_mm = %.2f", x);
    } else if (!strcmp(cle, "pas")) {
        if (x < cfg::PAS_MIN || x > cfg::PAS_MAX) {
            snprintf(tampon, sizeof tampon, "pas : de %.0f à %.0f mm", cfg::PAS_MIN, cfg::PAS_MAX);
            message = tampon;
            return false;
        }
        reglages.pas_note_mm = x;
        snprintf(tampon, sizeof tampon, "pas_note_mm = %.3f", x);
    } else if (!strcmp(cle, "inverse")) {
        if (!entier || x < 0 || x > 1) { message = "inverse : 0 ou 1"; return false; }
        reglages.sens_inverse = x;
        snprintf(tampon, sizeof tampon, "sens_inverse = %d", (int)x);
    } else if (!strcmp(cle, "lum")) {
        if (!entier || x < 0 || x > 255) { message = "luminosité : 0 à 255"; return false; }
        reglages.luminosite = x;
        snprintf(tampon, sizeof tampon, "luminosite = %d", (int)x);
    } else if (!strcmp(cle, "nbleds")) {
        if (!entier || x < 1 || x > cfg::NB_LEDS_MAX) { message = "nbleds : 1 à 300"; return false; }
        tests::arreter();
        reglages.nb_leds = x;
        snprintf(tampon, sizeof tampon, "nb_leds = %d", (int)x);
    } else if (!strcmp(cle, "ledsparnote")) {
        if (x != 1 && x != 2) { message = "ledsparnote : 1 ou 2"; return false; }
        reglages.leds_par_note = x;
        snprintf(tampon, sizeof tampon, "leds_par_note = %d", (int)x);
    } else if (!strcmp(cle, "basse")) {
        if (!entier || x < 0 || x > 127 - (cfg::NB_NOTES - 1)) { message = "basse : 0 à 79"; return false; }
        reglages.note_basse = x;
        char nom[12];
        snprintf(tampon, sizeof tampon, "note_basse = %d (%s)", (int)x, nomNote(x, nom, sizeof nom));
    } else if (!strcmp(cle, "antic")) {
        if (!entier || x < 0 || x > cfg::ANTICIPATION_MAX_MS) { message = "anticipation : 0 à 2000 ms"; return false; }
        reglages.anticipation_ms = x;
        snprintf(tampon, sizeof tampon, "anticipation = %d ms", (int)x);
        message = tampon;
        reglageModifie(false);
        return true;
    } else if (!strcmp(cle, "horsplage")) {
        if (x != 0 && x != 1) { message = "horsplage : 0 (ignorer) ou 1 (transposer)"; return false; }
        reglages.hors_plage = x;
        message = x ? "notes hors clavier : transposées" : "notes hors clavier : ignorées";
        reglageModifie(false);
        return true;
    } else if (!strcmp(cle, "mout")) {
        if (x != 0 && x != 1) { message = "mout : 0 ou 1"; return false; }
        reglages.midi_out = x;
        message = x ? "envoi au MX49 activé" : "envoi au MX49 coupé";
        reglageModifie(false);
        return true;
    } else {
        message = String("réglage inconnu : ") + cle;
        return false;
    }
    message = tampon;
    reglageModifie(true);
    return true;
}

bool modifierCouleur(const char* main, const char* hex, String& message) {
    if (*hex == '#') hex++;
    char* fin = nullptr;
    const unsigned long v = strtoul(hex, &fin, 16);
    if (strlen(hex) != 6 || !fin || *fin) {
        message = "couleur au format RRGGBB";
        return false;
    }
    if (!strcmp(main, "md")) reglages.couleur_md = v;
    else if (!strcmp(main, "mg")) reglages.couleur_mg = v;
    else {
        message = "main : md ou mg";
        return false;
    }
    message = String("couleur ") + main + " = #" + hex;
    reglageModifie(false);
    return true;
}

static void afficherConfig() {
    afficherReglages();
    const bandeau::LedsNote a = bandeau::ledsDeNote(0);
    const bandeau::LedsNote b = bandeau::ledsDeNote(cfg::NB_NOTES - 1);
    Serial.printf("  -> 1re note : %.2f mm, LED %d ; dernière : %.2f mm, LED %d\n", bandeau::positionNote(0), a.a,
                  bandeau::positionNote(cfg::NB_NOTES - 1), b.a);
    Serial.printf("  écoute MIDI : %s\n", midiin::ecouteActive() ? "active" : "coupée");
}

// ---------- Calibration ----------

static void afficherCalib() {
    tests::arreter();
    const bandeau::LedsNote l = bandeau::ledsDepuisPosition(posCalib);
    bandeau::fondLeds(l, CRGB::Red);
    Serial.printf("  position %.2f mm -> LED %d", posCalib, l.a);
    if (l.b >= 0) Serial.printf(" + %d", l.b);
    Serial.println(l.a < 0 && l.b < 0 ? "  (hors bandeau !)" : "");
}

static void demarrerCalib() {
    moteur::stop();
    etapeCalib = 1;
    posCalib = bandeau::positionNote(0);
    Serial.println("=== Calibration ===");
    Serial.println("Règle d'abord 'inverse' si le bandeau va dans le mauvais sens.");
    Serial.println("Étape 1/2 : la LED rouge doit être au-dessus du CENTRE de la touche la plus à GAUCHE (do).");
    Serial.println("  +        décale d'une LED vers la droite du clavier");
    Serial.println("  -        décale d'une LED vers la gauche");
    Serial.println("  + 5      décale de 5 LED (les décimales sont admises : + 0.5)");
    Serial.println("  ok       valide l'étape      annuler   abandonne sans rien changer");
    afficherCalib();
}

static void traiterCalib(const char* cmd) {
    if (strcmp(cmd, "annuler") == 0) {
        etapeCalib = 0;
        tests::arreter();
        Serial.println("Calibration annulée, réglages inchangés.");
        return;
    }
    if (cmd[0] == '+' || cmd[0] == '-') {
        const float signe = (cmd[0] == '+') ? 1.0f : -1.0f;
        const char* reste = cmd + 1;
        while (*reste == ' ') reste++;
        float n = 1.0f;
        if (*reste && !lireNombre(reste, n)) {
            Serial.println("Nombre de LED invalide. Exemples : +   -   + 3   - 0.5");
            return;
        }
        posCalib += signe * n * cfg::PAS_LED_MM;
        afficherCalib();
        return;
    }
    if (strcmp(cmd, "ok") == 0) {
        if (etapeCalib == 1) {
            posGauche = posCalib;
            etapeCalib = 2;
            posCalib = posGauche + (cfg::NB_NOTES - 1) * reglages.pas_note_mm;
            Serial.println("Étape 2/2 : la LED rouge doit être au-dessus du CENTRE de la touche la plus à DROITE (do).");
            afficherCalib();
            return;
        }
        const float pas = (posCalib - posGauche) / (cfg::NB_NOTES - 1);
        if (pas < cfg::PAS_MIN || pas > cfg::PAS_MAX || posGauche < cfg::OFFSET_MIN || posGauche > cfg::OFFSET_MAX) {
            Serial.printf("Résultat incohérent (offset %.2f mm, pas %.3f mm). Vérifie 'inverse' et recommence.\n",
                          posGauche, pas);
            etapeCalib = 0;
            tests::arreter();
            return;
        }
        etapeCalib = 0;
        reglages.offset_mm = posGauche;
        reglages.pas_note_mm = pas;
        Serial.printf("Calibration terminée : offset_mm = %.2f, pas_note_mm = %.3f\n", posGauche, pas);
        reglageModifie(true);
        return;
    }
    Serial.println("Calibration en cours : +, -, + N, - N, ok ou annuler.");
}

// ---------- Interprétation d'une ligne ----------

static void traiterLigne(char* l) {
    // Nettoyage : espaces de début/fin
    while (*l == ' ' || *l == '\t') l++;
    size_t n = strlen(l);
    while (n && (l[n - 1] == ' ' || l[n - 1] == '\t')) l[--n] = '\0';
    if (!n) return;

    // Séparation commande / argument ; seule la commande passe en minuscules (pas les noms de fichiers)
    char* arg = strchr(l, ' ');
    if (arg) {
        *arg++ = '\0';
        while (*arg == ' ') arg++;
    } else {
        arg = l + n;  // chaîne vide
    }
    for (char* p = l; *p; p++) *p = tolower((unsigned char)*p);
    const char* cmd = l;

    if (etapeCalib) {
        // En calibration, « + 3 » arrive découpé en « + » et « 3 » : on recolle
        char tout[96];
        snprintf(tout, sizeof tout, "%s%s%s", cmd, *arg ? " " : "", arg);
        traiterCalib(tout);
        return;
    }

    float x;
    long v;
    String msg;

    if (midiin::attenteNoteBasse() && strcmp(cmd, "annuler") == 0) {
        midiin::attendreNoteBasse(false);
        Serial.println("Attente de la touche la plus à gauche annulée.");
    } else if (!strcmp(cmd, "aide") || !strcmp(cmd, "?") || !strcmp(cmd, "help")) {
        afficherAide();
    } else if (!strcmp(cmd, "config")) {
        afficherConfig();
    } else if (!strcmp(cmd, "couleurs")) {
        tests::couleurs();
    } else if (!strcmp(cmd, "leds")) {
        long ms = 80;
        if (*arg && !lireEntier(arg, 10, 5000, ms)) { Serial.println("Usage : leds [ms]  (10 à 5000)"); return; }
        tests::chenillardLeds(ms);
    } else if (!strcmp(cmd, "notes")) {
        long ms = 300;
        if (*arg && !lireEntier(arg, 10, 5000, ms)) { Serial.println("Usage : notes [ms]  (10 à 5000)"); return; }
        tests::chenillardNotes(ms);
    } else if (!strcmp(cmd, "note")) {
        if (!lireEntier(arg, 0, cfg::NB_NOTES - 1, v)) { Serial.println("Usage : note N  (0 à 48)"); return; }
        tests::noteSeule(v);
    } else if (!strcmp(cmd, "apercu") || !strcmp(cmd, "aperçu")) {
        tests::apercu();
    } else if (!strcmp(cmd, "stop")) {
        tests::arreter();
        moteur::stop();
        Serial.println("Arrêté.");
    } else if (!strcmp(cmd, "midiout")) {
        tests::gammeMidiOut();
    } else if (!strcmp(cmd, "offset") || !strcmp(cmd, "pas") || !strcmp(cmd, "inverse") || !strcmp(cmd, "lum") ||
               !strcmp(cmd, "nbleds") || !strcmp(cmd, "ledsparnote") || !strcmp(cmd, "antic") ||
               !strcmp(cmd, "horsplage") || !strcmp(cmd, "mout") || (!strcmp(cmd, "basse") && *arg)) {
        if (!lireNombre(arg, x)) { Serial.printf("Usage : %s VALEUR (voir 'aide')\n", cmd); return; }
        const bool ok = modifierReglage(cmd, x, msg);
        Serial.println(ok ? msg + " — sauvegardé." : "Refusé : " + msg);
    } else if (!strcmp(cmd, "couleur")) {
        char main[4] = "";
        const char* hex = "";
        if (strlen(arg) > 3 && arg[2] == ' ') {
            strlcpy(main, arg, 3);
            hex = arg + 3;
        }
        Serial.println(modifierCouleur(main, hex, msg) ? msg : "Usage : couleur md|mg RRGGBB");
    } else if (!strcmp(cmd, "basse")) {
        midiin::attendreNoteBasse(true);
        Serial.println("Appuie sur la touche la plus à GAUCHE du MX49... ('annuler' pour abandonner)");
    } else if (!strcmp(cmd, "calib")) {
        demarrerCalib();
    } else if (!strcmp(cmd, "ecoute")) {
        if (!lireEntier(arg, 0, 1, v)) { Serial.println("Usage : ecoute 0|1"); return; }
        midiin::activerEcoute(v);
        Serial.printf("Écoute MIDI %s.\n", v ? "active" : "coupée");
    } else if (!strcmp(cmd, "trace")) {
        if (!lireEntier(arg, 0, 2, v)) { Serial.println("Usage : trace 0|1|2"); return; }
        midiin::activerTrace(v == 1);
        midiin::activerTraceBrute(v == 2);
        Serial.println(v == 2 ? "Trace brute : tous les messages MIDI reçus, en hexadécimal."
                              : v ? "Affichage des notes reçues activé." : "Affichage des notes reçues coupé.");
    } else if (!strcmp(cmd, "defaut") || !strcmp(cmd, "défaut")) {
        if (strcmp(arg, "oui") != 0) { Serial.println("Pour tout remettre par défaut, tape : defaut oui"); return; }
        tests::arreter();
        reglagesParDefaut(reglages);
        Serial.println("Réglages par défaut rétablis.");
        reglageModifie(true);
    } else if (!strcmp(cmd, "fichiers")) {
        String json;
        stockage::listerJson(json);
        Serial.println(json);
    } else if (!strcmp(cmd, "jouer")) {
        if (!*arg) { Serial.println("Usage : jouer NOM.mid"); return; }
        tests::arreter();
        if (moteur::charger(arg, msg)) moteur::lecture();
        else Serial.println("Impossible de charger : " + msg);
    } else if (!strcmp(cmd, "pause")) {
        if (moteur::etat() == moteur::Etat::Pause) moteur::lecture();
        else moteur::pause();
        Serial.printf("Lecture : %s\n", moteur::nomEtat());
    } else if (!strcmp(cmd, "etat")) {
        String json;
        moteur::etatJson(json);
        Serial.println(json);
    } else if (!strcmp(cmd, "wifi")) {
        Serial.println("Wi-Fi : " + reseau::description());
    } else {
        Serial.printf("Commande inconnue : '%s'. Tape 'aide'.\n", cmd);
    }
}

void boucle() {
    while (Serial.available()) {
        const char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (longueur) {
                ligne[longueur] = '\0';
                longueur = 0;
                traiterLigne(ligne);
            }
        } else if (c == 8 || c == 127) {  // retour arrière
            if (longueur) longueur--;
        } else if (longueur < sizeof(ligne) - 1) {
            ligne[longueur++] = c;
        }
    }
}

}  // namespace commandes
