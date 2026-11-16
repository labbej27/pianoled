// stockage.h — fichiers .mid dans LittleFS (dossier /midi)
#pragma once
#include <Arduino.h>
#include <FS.h>
#include "lecteur_smf.h"

namespace stockage {

constexpr uint32_t TAILLE_MAX_FICHIER = 256 * 1024;  // au-delà, le morceau ne tiendrait pas en RAM
constexpr size_t LONGUEUR_NOM_MAX = 48;

bool demarrer();
bool nomValide(const char* nom);          // .mid / .midi, sans '/', longueur raisonnable
String chemin(const char* nom);           // "/midi/" + nom
void listerJson(String& sortie);          // {"fichiers":[{"n":..,"o":..}],"libre":..,"total":..}
bool existe(const char* nom);
bool supprimer(const char* nom);

// Envoi d'un fichier en plusieurs morceaux (appelé par le serveur web)
bool debutEnvoi(const char* nom, String& erreur);
bool ecrireEnvoi(const uint8_t* donnees, size_t n, String& erreur);
bool finEnvoi(String& erreur);
void annulerEnvoi();

// Accès au fichier pour l'analyseur SMF, sans le charger en entier
class SourceFichier : public smf::Source {
public:
    explicit SourceFichier(const char* nom);
    ~SourceFichier() override;
    bool ouvert() const { return (bool)f_; }
    uint32_t taille() const override { return taille_; }
    bool lire(uint32_t pos, uint8_t* dst, uint32_t n) override;

private:
    File f_;
    uint32_t taille_ = 0;
};

}  // namespace stockage
