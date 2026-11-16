// stockage.cpp — LittleFS : liste, envoi, suppression des fichiers MIDI
#include "stockage.h"
#include <LittleFS.h>

namespace stockage {

static const char* DOSSIER = "/midi";
static const char* FICHIER_TEMPORAIRE = "/midi/.envoi.tmp";
static File envoi;
static String nomEnvoi;
static uint32_t tailleEnvoi = 0;

bool demarrer() {
    if (!LittleFS.begin(true)) {  // formate la partition au premier démarrage
        Serial.println("ERREUR : LittleFS indisponible.");
        return false;
    }
    if (!LittleFS.exists(DOSSIER)) LittleFS.mkdir(DOSSIER);
    LittleFS.remove(FICHIER_TEMPORAIRE);  // reste d'un envoi interrompu
    Serial.printf("LittleFS : %u Ko utilisés sur %u Ko.\n", (unsigned)(LittleFS.usedBytes() / 1024),
                  (unsigned)(LittleFS.totalBytes() / 1024));
    return true;
}

static bool finitPar(const char* s, const char* suffixe) {
    const size_t a = strlen(s), b = strlen(suffixe);
    return a >= b && strcasecmp(s + a - b, suffixe) == 0;
}

bool nomValide(const char* nom) {
    if (!nom || !*nom || nom[0] == '.') return false;
    const size_t n = strlen(nom);
    if (n > LONGUEUR_NOM_MAX) return false;
    if (strchr(nom, '/') || strchr(nom, '\\') || strchr(nom, '"')) return false;
    return finitPar(nom, ".mid") || finitPar(nom, ".midi");
}

String chemin(const char* nom) { return String(DOSSIER) + "/" + nom; }

// Échappement minimal pour JSON
static void ajouterChaineJson(String& s, const char* t) {
    s += '"';
    for (; *t; t++) {
        if (*t == '"' || *t == '\\') s += '\\';
        if ((uint8_t)*t >= 0x20) s += *t;
    }
    s += '"';
}

void listerJson(String& sortie) {
    sortie = "{\"fichiers\":[";
    File dossier = LittleFS.open(DOSSIER);
    bool premier = true;
    if (dossier && dossier.isDirectory()) {
        for (File f = dossier.openNextFile(); f; f = dossier.openNextFile()) {
            const char* nom = f.name();
            if (f.isDirectory() || !nomValide(nom)) continue;
            if (!premier) sortie += ',';
            premier = false;
            sortie += "{\"n\":";
            ajouterChaineJson(sortie, nom);
            sortie += ",\"o\":";
            sortie += (unsigned)f.size();
            sortie += '}';
        }
    }
    sortie += "],\"libre\":";
    sortie += (unsigned)(LittleFS.totalBytes() - LittleFS.usedBytes());
    sortie += ",\"total\":";
    sortie += (unsigned)LittleFS.totalBytes();
    sortie += '}';
}

bool existe(const char* nom) { return nomValide(nom) && LittleFS.exists(chemin(nom)); }

bool supprimer(const char* nom) { return nomValide(nom) && LittleFS.remove(chemin(nom)); }

bool debutEnvoi(const char* nom, String& erreur) {
    annulerEnvoi();
    if (!nomValide(nom)) {
        erreur = "nom de fichier refusé (extension .mid ou .midi, 48 caractères au plus)";
        return false;
    }
    envoi = LittleFS.open(FICHIER_TEMPORAIRE, FILE_WRITE);
    if (!envoi) {
        erreur = "impossible de créer le fichier";
        return false;
    }
    nomEnvoi = nom;
    tailleEnvoi = 0;
    return true;
}

bool ecrireEnvoi(const uint8_t* donnees, size_t n, String& erreur) {
    if (!envoi) {
        erreur = "aucun envoi en cours";
        return false;
    }
    tailleEnvoi += n;
    if (tailleEnvoi > TAILLE_MAX_FICHIER) {
        erreur = "fichier trop gros (256 Ko au plus)";
        annulerEnvoi();
        return false;
    }
    if (envoi.write(donnees, n) != n) {
        erreur = "mémoire flash pleine";
        annulerEnvoi();
        return false;
    }
    return true;
}

bool finEnvoi(String& erreur) {
    if (!envoi) {
        if (!erreur.length()) erreur = "aucun envoi en cours";
        return false;
    }
    envoi.close();
    const String dest = chemin(nomEnvoi.c_str());
    LittleFS.remove(dest);
    if (!LittleFS.rename(FICHIER_TEMPORAIRE, dest)) {
        erreur = "impossible d'enregistrer le fichier";
        LittleFS.remove(FICHIER_TEMPORAIRE);
        return false;
    }
    Serial.printf("Fichier reçu : %s (%u octets)\n", nomEnvoi.c_str(), (unsigned)tailleEnvoi);
    return true;
}

void annulerEnvoi() {
    if (envoi) envoi.close();
    LittleFS.remove(FICHIER_TEMPORAIRE);
}

// ---------- Source pour l'analyseur ----------

SourceFichier::SourceFichier(const char* nom) {
    if (!nomValide(nom)) return;
    f_ = LittleFS.open(chemin(nom), FILE_READ);
    if (f_) taille_ = f_.size();
}

SourceFichier::~SourceFichier() {
    if (f_) f_.close();
}

bool SourceFichier::lire(uint32_t pos, uint8_t* dst, uint32_t n) {
    if (!f_ || pos > taille_ || n > taille_ - pos) return false;
    if (f_.position() != pos && !f_.seek(pos)) return false;
    return f_.read(dst, n) == n;
}

}  // namespace stockage
