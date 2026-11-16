// lecteur_smf.h — analyseur de fichiers MIDI standard (SMF format 0 et 1)
//
// C++ pur, sans dépendance Arduino : compile sur l'ESP32 et sur PC (tests unitaires).
// Le fichier est lu en deux passes à travers une Source (pas de chargement complet en RAM) :
//   1. repérage des pistes, relevé des changements de tempo, comptage des notes ;
//   2. lecture des notes, conversion ticks → microsecondes, fusion en une timeline triée.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace smf {

constexpr int MAX_PISTES = 32;
constexpr int MAX_TEMPOS = 256;
constexpr int LONGUEUR_NOM = 24;
constexpr uint32_t MAX_EVENEMENTS_DEFAUT = 16000;  // 8 octets chacun → 128 Ko

// Un appui ou un relâchement de note, à un instant absolu du morceau
struct Evenement {
    uint32_t temps_us;  // depuis le début du morceau, au tempo d'origine
    uint8_t note;
    uint8_t velocite;   // 0 = relâchement (Note Off ou Note On de vélocité 0)
    uint8_t piste;
    uint8_t canal;      // 0..15
};

struct ChangementTempo {
    uint32_t tick;
    uint32_t temps_us;
    uint32_t us_par_noire;
};

struct InfoPiste {
    char nom[LONGUEUR_NOM];
    uint32_t nb_notes;       // nombre d'appuis
    uint8_t note_min;
    uint8_t note_max;
    uint16_t masque_canaux;  // bit n = canal n utilisé
    uint32_t somme_notes;    // pour la hauteur moyenne
};

enum class Erreur : uint8_t {
    Aucune,
    Lecture,
    PasUnFichierMidi,
    FormatNonGere,
    DivisionInvalide,
    TropDEvenements,
    MemoireInsuffisante,
    AucuneNote,
};

const char* texteErreur(Erreur e);

// Accès aux octets du fichier (fichier LittleFS sur l'ESP32, FILE* ou mémoire sur PC)
class Source {
public:
    virtual ~Source() = default;
    virtual uint32_t taille() const = 0;
    // Copie n octets à partir de pos ; renvoie false si la lecture est impossible
    virtual bool lire(uint32_t pos, uint8_t* dst, uint32_t n) = 0;
};

// Source sur un tableau en mémoire
class SourceMemoire : public Source {
public:
    SourceMemoire(const uint8_t* donnees, uint32_t taille) : d_(donnees), n_(taille) {}
    uint32_t taille() const override { return n_; }
    bool lire(uint32_t pos, uint8_t* dst, uint32_t n) override;

private:
    const uint8_t* d_;
    uint32_t n_;
};

struct Morceau {
    Evenement* evenements = nullptr;  // triés par temps (relâchements avant appuis au même instant)
    uint32_t nb_evenements = 0;
    ChangementTempo tempos[MAX_TEMPOS];
    uint16_t nb_tempos = 0;
    InfoPiste pistes[MAX_PISTES];
    uint8_t nb_pistes = 0;
    uint16_t format = 0;
    uint16_t division = 0;  // ticks par noire (ou SMPTE si bit 15)
    uint32_t duree_us = 0;

    Morceau() = default;
    Morceau(const Morceau&) = delete;
    Morceau& operator=(const Morceau&) = delete;
    ~Morceau() { liberer(); }
    void liberer();

    // Tempo en vigueur à un instant donné (µs par noire ; 500000 = 120 BPM)
    uint32_t tempoA(uint32_t temps_us) const;
};

// Analyse complète. En cas d'erreur, le morceau est laissé vide.
Erreur analyser(Source& src, Morceau& m, uint32_t max_evenements = MAX_EVENEMENTS_DEFAUT);

}  // namespace smf
