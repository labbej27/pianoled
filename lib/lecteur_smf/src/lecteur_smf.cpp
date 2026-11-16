// lecteur_smf.cpp — analyseur SMF format 0 et 1
#include "lecteur_smf.h"
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <new>

namespace smf {

const char* texteErreur(Erreur e) {
    switch (e) {
        case Erreur::Aucune: return "aucune erreur";
        case Erreur::Lecture: return "erreur de lecture du fichier";
        case Erreur::PasUnFichierMidi: return "ce n'est pas un fichier MIDI (en-tête MThd absent)";
        case Erreur::FormatNonGere: return "format MIDI 2 non géré (seuls les formats 0 et 1 le sont)";
        case Erreur::DivisionInvalide: return "division temporelle invalide";
        case Erreur::TropDEvenements: return "trop de notes pour la mémoire de l'ESP32";
        case Erreur::MemoireInsuffisante: return "mémoire insuffisante";
        case Erreur::AucuneNote: return "le fichier ne contient aucune note";
    }
    return "erreur inconnue";
}

bool SourceMemoire::lire(uint32_t pos, uint8_t* dst, uint32_t n) {
    if (pos > n_ || n > n_ - pos) return false;
    memcpy(dst, d_ + pos, n);
    return true;
}

void Morceau::liberer() {
    free(evenements);
    evenements = nullptr;
    nb_evenements = 0;
    nb_tempos = 0;
    nb_pistes = 0;
    duree_us = 0;
}

uint32_t Morceau::tempoA(uint32_t temps_us) const {
    uint32_t t = 500000;
    for (uint16_t i = 0; i < nb_tempos && tempos[i].temps_us <= temps_us; i++) t = tempos[i].us_par_noire;
    return t;
}

namespace {

// Lecture séquentielle tamponnée d'une plage du fichier
class Curseur {
public:
    Curseur(Source& s, uint32_t debut, uint32_t fin) : src_(s), pos_(debut), fin_(fin) {}
    bool fini() const { return pos_ >= fin_ || erreur_; }
    bool erreur() const { return erreur_; }
    uint32_t pos() const { return pos_; }

    uint8_t octet() {
        if (pos_ >= fin_) {
            erreur_ = true;  // fin de piste inattendue
            return 0;
        }
        if (pos_ < tamponDebut_ || pos_ >= tamponDebut_ + tamponN_) {
            uint32_t n = fin_ - pos_;
            if (n > sizeof tampon_) n = sizeof tampon_;
            if (!src_.lire(pos_, tampon_, n)) {
                erreur_ = true;
                lectureImpossible_ = true;
                return 0;
            }
            tamponDebut_ = pos_;
            tamponN_ = n;
        }
        return tampon_[pos_++ - tamponDebut_];
    }

    // Quantité de longueur variable (au plus 4 octets)
    uint32_t vlq() {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            const uint8_t b = octet();
            v = (v << 7) | (b & 0x7F);
            if (!(b & 0x80)) return v;
        }
        erreur_ = true;  // VLQ trop long : fichier corrompu
        return v;
    }

    void sauter(uint32_t n) {
        if (n > fin_ - pos_) {
            pos_ = fin_;
            erreur_ = true;
        } else {
            pos_ += n;
        }
    }

    bool lectureImpossible() const { return lectureImpossible_; }

private:
    Source& src_;
    uint32_t pos_, fin_;
    uint8_t tampon_[128];
    uint32_t tamponDebut_ = 0, tamponN_ = 0;
    bool erreur_ = false;
    bool lectureImpossible_ = false;
};

struct PlagePiste {
    uint32_t debut, fin;
};

uint32_t lire32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
uint16_t lire16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }

// Parcourt une piste et appelle :
//   surNote(tick, canal, note, velocite)   velocite 0 = relâchement
//   surTempo(tick, us_par_noire)
//   surNom(texte, longueur)
// Running status géré ; SysEx et méta-événements inutiles ignorés.
// Une piste corrompue est lue jusqu'à l'erreur, sans faire échouer tout le fichier.
template <class FNote, class FTempo, class FNom>
bool parcourirPiste(Source& src, const PlagePiste& p, FNote surNote, FTempo surTempo, FNom surNom) {
    Curseur c(src, p.debut, p.fin);
    uint32_t tick = 0;
    uint8_t statut = 0;
    while (!c.fini()) {
        tick += c.vlq();
        if (c.erreur()) break;
        uint8_t b = c.octet();
        if (b == 0xFF) {  // méta-événement
            const uint8_t type = c.octet();
            const uint32_t lg = c.vlq();
            if (c.erreur()) break;
            if (type == 0x2F) break;  // fin de piste
            if (type == 0x51 && lg == 3) {
                const uint8_t a = c.octet(), b2 = c.octet(), d = c.octet();
                const uint32_t t = (uint32_t)a << 16 | (uint32_t)b2 << 8 | d;
                if (t > 0) surTempo(tick, t);
            } else if ((type == 0x03 || type == 0x04) && lg > 0) {
                char nom[LONGUEUR_NOM];
                const uint32_t n = lg < LONGUEUR_NOM - 1 ? lg : LONGUEUR_NOM - 1;
                for (uint32_t i = 0; i < n; i++) nom[i] = (char)c.octet();
                c.sauter(lg - n);
                surNom(type, nom, n);
            } else {
                c.sauter(lg);
            }
        } else if (b == 0xF0 || b == 0xF7) {  // SysEx : ignoré
            c.sauter(c.vlq());
        } else {
            uint8_t d1;
            if (b & 0x80) {
                if (b >= 0xF0) break;  // octet système interdit dans un SMF : piste corrompue
                statut = b;
                d1 = c.octet();
            } else {
                if (!statut) break;  // donnée sans statut : piste corrompue
                d1 = b;              // running status
            }
            const uint8_t type = statut & 0xF0, canal = statut & 0x0F;
            uint8_t d2 = 0;
            if (type != 0xC0 && type != 0xD0) d2 = c.octet();
            if (c.erreur()) break;
            if (type == 0x90) surNote(tick, canal, d1 & 0x7F, d2 & 0x7F);
            else if (type == 0x80) surNote(tick, canal, d1 & 0x7F, 0);
        }
    }
    return !c.lectureImpossible();
}

// Conversion ticks → µs à l'aide de la carte des tempos
struct Horloge {
    const Morceau& m;
    bool smpte;
    double us_par_tick_smpte;

    uint32_t versUs(uint32_t tick) const {
        if (smpte) return (uint32_t)(tick * us_par_tick_smpte + 0.5);
        // Dernier changement de tempo au plus tard à ce tick
        int k = -1;
        for (int i = 0; i < m.nb_tempos && m.tempos[i].tick <= tick; i++) k = i;
        uint32_t tick0 = 0, us0 = 0, tempo = 500000;
        if (k >= 0) {
            tick0 = m.tempos[k].tick;
            us0 = m.tempos[k].temps_us;
            tempo = m.tempos[k].us_par_noire;
        }
        return us0 + (uint32_t)((uint64_t)(tick - tick0) * tempo / m.division);
    }
};

}  // namespace

Erreur analyser(Source& src, Morceau& m, uint32_t max_evenements) {
    m.liberer();

    // --- En-tête ---
    uint8_t h[14];
    if (src.taille() < 14 || !src.lire(0, h, 14)) return Erreur::PasUnFichierMidi;
    if (memcmp(h, "MThd", 4) != 0 || lire32(h + 4) < 6) return Erreur::PasUnFichierMidi;
    m.format = lire16(h + 8);
    const uint16_t nbPistesAnnonce = lire16(h + 10);
    m.division = lire16(h + 12);
    if (m.format > 1) return Erreur::FormatNonGere;

    Horloge horloge{m, (m.division & 0x8000) != 0, 0.0};
    if (horloge.smpte) {
        int ips = -(int8_t)(m.division >> 8);   // images par seconde : 24, 25, 29 (29,97), 30
        const int tpi = m.division & 0xFF;      // ticks par image
        if (ips <= 0 || tpi == 0) return Erreur::DivisionInvalide;
        const double ipsReel = (ips == 29) ? 29.97 : ips;
        horloge.us_par_tick_smpte = 1e6 / (ipsReel * tpi);
    } else if (m.division == 0) {
        return Erreur::DivisionInvalide;
    }

    // --- Repérage des pistes (les blocs inconnus sont sautés) ---
    PlagePiste plages[MAX_PISTES];
    uint8_t nbPlages = 0;
    uint32_t pos = 8 + lire32(h + 4);
    while (pos + 8 <= src.taille() && nbPlages < MAX_PISTES && nbPlages < nbPistesAnnonce) {
        uint8_t bloc[8];
        if (!src.lire(pos, bloc, 8)) return Erreur::Lecture;
        uint32_t lg = lire32(bloc + 4);
        const uint32_t debut = pos + 8;
        if (lg > src.taille() - debut) lg = src.taille() - debut;  // fichier tronqué : on lit ce qui existe
        if (memcmp(bloc, "MTrk", 4) == 0) plages[nbPlages++] = {debut, debut + lg};
        pos = debut + lg;
    }
    m.nb_pistes = nbPlages;
    for (int i = 0; i < nbPlages; i++) {
        InfoPiste& ip = m.pistes[i];
        memset(&ip, 0, sizeof ip);
        ip.note_min = 127;
    }

    // --- Passe 1 : tempos, noms, comptage des notes ---
    uint32_t nbEvenements = 0;
    bool tempoTropNombreux = false;
    for (uint8_t p = 0; p < nbPlages; p++) {
        bool nomTrouve = false;
        const bool ok = parcourirPiste(
            src, plages[p],
            [&](uint32_t, uint8_t, uint8_t, uint8_t) { nbEvenements++; },
            [&](uint32_t tick, uint32_t t) {
                if (horloge.smpte) return;
                if (m.nb_tempos < MAX_TEMPOS) m.tempos[m.nb_tempos++] = {tick, 0, t};
                else tempoTropNombreux = true;
            },
            [&](uint8_t type, const char* nom, uint32_t n) {
                // Le nom de piste (0x03) a priorité sur le nom d'instrument (0x04)
                if (nomTrouve && type != 0x03) return;
                memcpy(m.pistes[p].nom, nom, n);
                m.pistes[p].nom[n] = '\0';
                nomTrouve = (type == 0x03);
            });
        if (!ok) return Erreur::Lecture;
    }
    (void)tempoTropNombreux;  // au-delà de MAX_TEMPOS, les derniers changements sont ignorés

    if (nbEvenements == 0) return Erreur::AucuneNote;
    if (nbEvenements > max_evenements) return Erreur::TropDEvenements;

    // Carte des tempos triée par tick, avec l'instant absolu de chaque changement
    std::stable_sort(m.tempos, m.tempos + m.nb_tempos,
                     [](const ChangementTempo& a, const ChangementTempo& b) { return a.tick < b.tick; });
    {
        uint32_t tick0 = 0, us0 = 0, tempo = 500000;
        for (uint16_t i = 0; i < m.nb_tempos; i++) {
            us0 += (uint32_t)((uint64_t)(m.tempos[i].tick - tick0) * tempo / m.division);
            m.tempos[i].temps_us = us0;
            tick0 = m.tempos[i].tick;
            tempo = m.tempos[i].us_par_noire;
        }
    }

    // --- Passe 2 : notes ---
    m.evenements = (Evenement*)malloc(sizeof(Evenement) * nbEvenements);
    if (!m.evenements) {
        m.liberer();
        return Erreur::MemoireInsuffisante;
    }
    uint32_t n = 0;
    for (uint8_t p = 0; p < nbPlages; p++) {
        InfoPiste& ip = m.pistes[p];
        parcourirPiste(
            src, plages[p],
            [&](uint32_t tick, uint8_t canal, uint8_t note, uint8_t vel) {
                if (n >= nbEvenements) return;
                m.evenements[n++] = {horloge.versUs(tick), note, vel, p, canal};
                if (vel) {
                    ip.nb_notes++;
                    ip.somme_notes += note;
                    if (note < ip.note_min) ip.note_min = note;
                    if (note > ip.note_max) ip.note_max = note;
                    ip.masque_canaux |= (uint16_t)(1u << canal);
                }
            },
            [](uint32_t, uint32_t) {}, [](uint8_t, const char*, uint32_t) {});
    }
    m.nb_evenements = n;

    // Fusion des pistes : tri par instant ; au même instant, les relâchements d'abord
    // (sinon une note répétée serait éteinte juste après avoir été rallumée)
    std::sort(m.evenements, m.evenements + n, [](const Evenement& a, const Evenement& b) {
        if (a.temps_us != b.temps_us) return a.temps_us < b.temps_us;
        const bool aOff = a.velocite == 0, bOff = b.velocite == 0;
        if (aOff != bOff) return aOff;
        if (a.piste != b.piste) return a.piste < b.piste;
        return a.note < b.note;
    });
    m.duree_us = n ? m.evenements[n - 1].temps_us : 0;
    return Erreur::Aucune;
}

}  // namespace smf
