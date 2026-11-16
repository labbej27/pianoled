// moteur_lecture.cpp — moteur de lecture : cadencement, mains, modes démo et attente, boucle A-B
#include "moteur_lecture.h"
#include <ArduinoJson.h>
#include "bandeau.h"
#include "config.h"
#include "lecteur_smf.h"
#include "midi_in.h"
#include "midi_out.h"
#include "stockage.h"

namespace moteur {

// Un groupe = les notes d'un même couple (piste, canal) ; c'est l'unité attribuée à une main
struct Groupe {
    uint8_t piste;
    uint8_t canal;
    uint16_t nb_notes;
    uint8_t min, max, moy;
    uint8_t main;
    char nom[smf::LONGUEUR_NOM + 16];
};

constexpr int MAX_GROUPES = 32;
constexpr int MAX_REQUIS = 16;
constexpr uint32_t TOLERANCE_ACCORD_US = 40000;  // notes à moins de 40 ms d'écart = même accord
constexpr uint32_t AVANCE_APPUI_MS = 250;        // un appui un peu en avance compte quand même
constexpr uint32_t PERIODE_AFFICHAGE_MS = 15;

static smf::Morceau morceau;
static char nomFichier[stockage::LONGUEUR_NOM_MAX + 1];
static Etat etat_ = Etat::Vide;
static Mode mode_ = Mode::Demo;
static uint8_t travail_ = TRAVAIL_DEUX;
static uint8_t vitesse_ = 100;

// Position dans le morceau en µs × 100 : on y ajoute dt × vitesse (%) à chaque tour
static uint64_t pos100 = 0;
static uint32_t dernierMicros = 0;
static uint32_t idx = 0;  // prochain événement à traiter

static Groupe groupes[MAX_GROUPES];
static uint8_t nbGroupes = 0;
static uint8_t groupeDe[smf::MAX_PISTES][16];  // 0xFF = aucun
static uint8_t partage_ = 60;                  // do4 : à partir de cette note, main droite

// Notes du morceau en cours, indexées par note du clavier (après transposition)
static uint8_t actifs[128];
static uint8_t mainActive[128];

static uint32_t pointA_ = UINT32_MAX, pointB_ = UINT32_MAX;
static bool boucle_ = false;

// Mode attente
static uint8_t requis[MAX_REQUIS];
static uint8_t nbRequis = 0;
static uint32_t debutAttente = 0;
static uint32_t erreurs = 0, justes = 0;

static bool affichageModifie = true;
static uint32_t dernierAffichage = 0;

static inline uint32_t positionUs() { return (uint32_t)(pos100 / 100); }

// ---------- Mains et transposition ----------

static uint8_t mainDe(const smf::Evenement& e) {
    if (e.piste >= smf::MAX_PISTES) return MAIN_MASQUEE;
    const uint8_t g = groupeDe[e.piste][e.canal & 0x0F];
    if (g == 0xFF) return MAIN_MASQUEE;
    uint8_t m = groupes[g].main;
    if (m == MAIN_HAUTEUR) m = (e.note >= partage_) ? MAIN_DROITE : MAIN_GAUCHE;
    return m;
}

// Note du clavier correspondant à une note du fichier, ou -1 si elle est ignorée
static int noteClavier(uint8_t note) {
    const int bas = reglages.note_basse, haut = bas + cfg::NB_NOTES - 1;
    int n = note;
    if (n >= bas && n <= haut) return n;
    if (reglages.hors_plage != HORS_PLAGE_TRANSPOSER) return -1;
    while (n < bas) n += 12;
    while (n > haut) n -= 12;
    return n;
}

static bool travaillee(uint8_t main) { return travail_ == TRAVAIL_DEUX || travail_ == main; }

// En démo, on envoie tout ; en attente, seulement l'accompagnement (mains non travaillées)
static bool envoyerAuMx49(uint8_t main) {
    return reglages.midi_out && (mode_ == Mode::Demo || !travaillee(main));
}

static bool aTravailler(const smf::Evenement& e) {
    if (!e.velocite) return false;
    const uint8_t m = mainDe(e);
    return m != MAIN_MASQUEE && travaillee(m) && noteClavier(e.note) >= 0;
}

// ---------- Application des événements ----------

static void appliquer(const smf::Evenement& e, bool sonore) {
    const uint8_t m = mainDe(e);
    if (m == MAIN_MASQUEE) return;
    const int k = noteClavier(e.note);
    if (e.velocite) {
        if (k >= 0) {
            if (actifs[k] < 255) actifs[k]++;
            mainActive[k] = m;
        }
        if (sonore && envoyerAuMx49(m)) midiout::noteOn(e.note, e.velocite);
    } else {
        if (k >= 0 && actifs[k]) actifs[k]--;
        if (sonore && envoyerAuMx49(m)) midiout::noteOff(e.note);
    }
    affichageModifie = true;
}

static void reinitialiserNotes() {
    memset(actifs, 0, sizeof actifs);
    midiout::toutCouper();
    nbRequis = 0;
    affichageModifie = true;
}

// Se place à un instant donné et reconstruit les notes tenues (sans son)
static void positionner(uint32_t us) {
    reinitialiserNotes();
    idx = 0;
    while (idx < morceau.nb_evenements && morceau.evenements[idx].temps_us < us) {
        appliquer(morceau.evenements[idx++], false);
    }
    pos100 = (uint64_t)us * 100;
    dernierMicros = micros();
}

// ---------- Mode attente ----------

static bool estRequis(uint8_t note) {
    for (uint8_t i = 0; i < nbRequis; i++)
        if (requis[i] == note) return true;
    return false;
}

// Arrêt sur l'accord qui commence à l'événement idx : on l'affiche, puis on attend le joueur
static void entrerAttente() {
    const uint32_t fin = morceau.evenements[idx].temps_us + TOLERANCE_ACCORD_US;
    nbRequis = 0;
    while (idx < morceau.nb_evenements && morceau.evenements[idx].temps_us <= fin) {
        const smf::Evenement& e = morceau.evenements[idx];
        if (aTravailler(e) && nbRequis < MAX_REQUIS) {
            const uint8_t k = (uint8_t)noteClavier(e.note);
            if (!estRequis(k)) requis[nbRequis++] = k;
        }
        appliquer(e, true);
        idx++;
    }
    pos100 = (uint64_t)fin * 100;
    etat_ = Etat::AttenteJoueur;
    debutAttente = millis();
    affichageModifie = true;
}

// Toutes les notes requises enfoncées ensemble, chacune appuyée depuis le début de l'attente
static bool accordJoue() {
    const uint32_t depuis = debutAttente - AVANCE_APPUI_MS;
    for (uint8_t i = 0; i < nbRequis; i++) {
        const uint8_t k = requis[i];
        if (!midiin::enfoncee(k)) return false;
        if ((int32_t)(midiin::dernierAppui(k) - depuis) < 0) return false;
    }
    return true;
}

// Une note à jouer arrive-t-elle très bientôt ? (le joueur est un peu en avance)
static bool attendueBientot(uint8_t note) {
    const uint32_t lim = positionUs() + AVANCE_APPUI_MS * 1000ULL * vitesse_ / 100;
    for (uint32_t j = idx; j < morceau.nb_evenements && morceau.evenements[j].temps_us <= lim; j++) {
        const smf::Evenement& e = morceau.evenements[j];
        if (aTravailler(e) && noteClavier(e.note) == note) return true;
    }
    return false;
}

// Appui sur une touche du MX49 (rappel de midi_in)
static void surNoteClavier(uint8_t note, uint8_t velocite) {
    if (!velocite || mode_ != Mode::Attente) return;
    if (etat_ != Etat::Lecture && etat_ != Etat::AttenteJoueur) return;
    const int i = (int)note - reglages.note_basse;
    if (i < 0 || i >= cfg::NB_NOTES) return;
    const bool bon = estRequis(note) || (actifs[note] && travaillee(mainActive[note])) || attendueBientot(note);
    if (bon) {
        justes++;
        bandeau::flash(i, CRGB(0, 255, 0), 150);
    } else {
        erreurs++;
        bandeau::flash(i, CRGB(255, 0, 0), 300);
    }
}

// ---------- Affichage ----------

static CRGB couleurMain(uint8_t main) {
    return CRGB(main == MAIN_DROITE ? reglages.couleur_md : reglages.couleur_mg);
}

static void rafraichirLeds() {
    if (etat_ == Etat::Vide) return;
    const uint32_t t = millis();
    const bool anime = etat_ == Etat::Lecture || etat_ == Etat::AttenteJoueur;
    if (!affichageModifie && !(anime && t - dernierAffichage >= PERIODE_AFFICHAGE_MS)) return;
    dernierAffichage = t;
    affichageModifie = false;

    // Anticipation : notes qui commencent dans la fenêtre à venir (durée réelle, donc × vitesse)
    static uint8_t anticipe[128];
    memset(anticipe, 0, sizeof anticipe);
    if (reglages.anticipation_ms && etat_ != Etat::Arret) {
        const uint32_t lim = positionUs() + (uint32_t)((uint64_t)reglages.anticipation_ms * 1000 * vitesse_ / 100);
        for (uint32_t j = idx; j < morceau.nb_evenements && morceau.evenements[j].temps_us <= lim; j++) {
            const smf::Evenement& e = morceau.evenements[j];
            if (!e.velocite) continue;
            const uint8_t m = mainDe(e);
            const int k = noteClavier(e.note);
            if (m != MAIN_MASQUEE && k >= 0 && !anticipe[k]) anticipe[k] = m;
        }
    }

    const uint8_t pulsation = beatsin8(80, 90, 255);  // notes attendues en mode attente
    for (int i = 0; i < cfg::NB_NOTES; i++) {
        const uint8_t note = reglages.note_basse + i;
        CRGB c = CRGB::Black;
        if (actifs[note]) {
            c = couleurMain(mainActive[note]);
            if (etat_ == Etat::AttenteJoueur && estRequis(note)) c.nscale8_video(pulsation);
        } else if (anticipe[note]) {
            c = couleurMain(anticipe[note]);
            c.nscale8_video(40);  // faible : la note arrive bientôt
        }
        bandeau::noteLecture(i, c);
    }
}

// ---------- Groupes et attribution automatique des mains ----------

static bool contientMot(const char* texte, const char* const* mots) {
    char mot[24];
    size_t n = 0;
    for (const char* p = texte;; p++) {
        const char c = *p;
        if (isalnum((unsigned char)c)) {
            if (n < sizeof mot - 1) mot[n++] = (char)tolower((unsigned char)c);
            continue;
        }
        if (n) {
            mot[n] = '\0';
            for (const char* const* m = mots; *m; m++)
                if (strcmp(mot, *m) == 0) return true;
            n = 0;
        }
        if (!c) return false;
    }
}

static void construireGroupes() {
    static uint16_t nb[smf::MAX_PISTES][16];
    static uint32_t somme[smf::MAX_PISTES][16];
    static uint8_t mn[smf::MAX_PISTES][16], mx[smf::MAX_PISTES][16];
    memset(nb, 0, sizeof nb);
    memset(somme, 0, sizeof somme);
    memset(mn, 127, sizeof mn);
    memset(mx, 0, sizeof mx);
    memset(groupeDe, 0xFF, sizeof groupeDe);

    for (uint32_t i = 0; i < morceau.nb_evenements; i++) {
        const smf::Evenement& e = morceau.evenements[i];
        if (!e.velocite || e.piste >= smf::MAX_PISTES) continue;
        const uint8_t p = e.piste, c = e.canal & 0x0F;
        nb[p][c]++;
        somme[p][c] += e.note;
        if (e.note < mn[p][c]) mn[p][c] = e.note;
        if (e.note > mx[p][c]) mx[p][c] = e.note;
    }

    nbGroupes = 0;
    for (uint8_t p = 0; p < morceau.nb_pistes; p++) {
        uint8_t canauxPiste = 0;
        for (uint8_t c = 0; c < 16; c++) canauxPiste += nb[p][c] ? 1 : 0;
        for (uint8_t c = 0; c < 16 && nbGroupes < MAX_GROUPES; c++) {
            if (!nb[p][c]) continue;
            Groupe& g = groupes[nbGroupes];
            g.piste = p;
            g.canal = c;
            g.nb_notes = nb[p][c];
            g.min = mn[p][c];
            g.max = mx[p][c];
            g.moy = (uint8_t)(somme[p][c] / nb[p][c]);
            g.main = MAIN_HAUTEUR;
            const char* base = morceau.pistes[p].nom;
            if (base[0] && canauxPiste == 1) {
                snprintf(g.nom, sizeof g.nom, "%s", base);
            } else if (base[0]) {
                snprintf(g.nom, sizeof g.nom, "%s (canal %u)", base, c + 1);
            } else if (morceau.nb_pistes > 1) {
                snprintf(g.nom, sizeof g.nom, "Piste %u, canal %u", p + 1, c + 1);
            } else {
                snprintf(g.nom, sizeof g.nom, "Canal %u", c + 1);
            }
            groupeDe[p][c] = nbGroupes++;
        }
    }
}

static void proposerMains() {
    static const char* const MOTS_DROITE[] = {"right", "droite", "rh", "md", "treble", "dextre", nullptr};
    static const char* const MOTS_GAUCHE[] = {"left", "gauche", "lh", "mg", "bass", "basse", "sinistre", nullptr};
    bool droiteNommee = false, gaucheNommee = false;
    uint8_t libres[MAX_GROUPES];
    uint8_t nbLibres = 0;

    for (uint8_t i = 0; i < nbGroupes; i++) {
        Groupe& g = groupes[i];
        if (g.canal == 9) {
            g.main = MAIN_MASQUEE;  // canal 10 : batterie
        } else if (contientMot(g.nom, MOTS_DROITE)) {
            g.main = MAIN_DROITE;
            droiteNommee = true;
        } else if (contientMot(g.nom, MOTS_GAUCHE)) {
            g.main = MAIN_GAUCHE;
            gaucheNommee = true;
        } else {
            g.main = MAIN_HAUTEUR;
            libres[nbLibres++] = i;
        }
    }

    if (droiteNommee && gaucheNommee) {
        // Les deux mains sont identifiées par leur nom : le reste est masqué
        for (uint8_t i = 0; i < nbLibres; i++) groupes[libres[i]].main = MAIN_MASQUEE;
    } else if (!droiteNommee && !gaucheNommee && nbLibres == 2) {
        // Deux parties : la plus aiguë à droite
        Groupe& a = groupes[libres[0]];
        Groupe& b = groupes[libres[1]];
        const bool aPlusHaut = a.moy >= b.moy;
        a.main = aPlusHaut ? MAIN_DROITE : MAIN_GAUCHE;
        b.main = aPlusHaut ? MAIN_GAUCHE : MAIN_DROITE;
    }
    // Sinon : partage par hauteur (réglable dans l'interface)
}

// ▶ : lecture / pause (bascule) ; ■ : arrêt et retour au début
static void surTransport(midiin::Transport t) {
    if (etat_ == Etat::Vide) {
        Serial.println("Bouton du MX49 : aucun morceau chargé.");
        return;
    }
    if (t == midiin::Transport::Stop) {
        stop();
        Serial.println("Bouton ■ du MX49 : arrêt, retour au début.");
    } else if (etat_ == Etat::Lecture || etat_ == Etat::AttenteJoueur) {
        pause();
        Serial.println("Bouton ▶ du MX49 : pause.");
    } else {
        lecture();
        Serial.println("Bouton ▶ du MX49 : lecture.");
    }
}

// ---------- Interface publique ----------

void demarrer() {
    memset(groupeDe, 0xFF, sizeof groupeDe);
    nomFichier[0] = '\0';
    midiin::definirRappel(surNoteClavier);
    midiin::definirRappelTransport(surTransport);
}

bool charger(const char* nom, String& erreur) {
    stop();
    stockage::SourceFichier src(nom);
    if (!src.ouvert()) {
        erreur = "fichier introuvable";
        return false;
    }
    const uint32_t t0 = millis();
    const smf::Erreur e = smf::analyser(src, morceau);
    if (e != smf::Erreur::Aucune) {
        etat_ = Etat::Vide;
        nomFichier[0] = '\0';
        nbGroupes = 0;
        bandeau::effacerLecture();
        erreur = smf::texteErreur(e);
        return false;
    }
    strlcpy(nomFichier, nom, sizeof nomFichier);
    construireGroupes();
    proposerMains();
    pointA_ = pointB_ = UINT32_MAX;
    boucle_ = false;
    erreurs = justes = 0;
    etat_ = Etat::Arret;
    positionner(0);
    const uint32_t s = morceau.duree_us / 1000000;
    Serial.printf("Morceau chargé : %s — %u événements, %u:%02u, %u groupe(s), analyse en %u ms\n", nom,
                  (unsigned)morceau.nb_evenements, (unsigned)(s / 60), (unsigned)(s % 60), nbGroupes,
                  (unsigned)(millis() - t0));
    return true;
}

void boucle() {
    const uint32_t maintenant = micros();
    const uint32_t dt = maintenant - dernierMicros;
    dernierMicros = maintenant;

    if (etat_ == Etat::Lecture) {
        pos100 += (uint64_t)dt * vitesse_;
        uint32_t pos = positionUs();
        if (boucle_ && pointB_ != UINT32_MAX && pointB_ > pointA_ && pos >= pointB_) {
            positionner(pointA_);
            pos = pointA_;
        }
        while (idx < morceau.nb_evenements && morceau.evenements[idx].temps_us <= pos) {
            if (mode_ == Mode::Attente && aTravailler(morceau.evenements[idx])) {
                entrerAttente();
                break;
            }
            appliquer(morceau.evenements[idx++], true);
        }
        if (etat_ == Etat::Lecture && idx >= morceau.nb_evenements) {
            Serial.println("Fin du morceau.");
            stop();
        }
    } else if (etat_ == Etat::AttenteJoueur && accordJoue()) {
        etat_ = Etat::Lecture;
        nbRequis = 0;
        dernierMicros = micros();
        affichageModifie = true;
    }
    rafraichirLeds();
}

void lecture() {
    if (etat_ == Etat::Vide || etat_ == Etat::Lecture || etat_ == Etat::AttenteJoueur) return;
    if (etat_ == Etat::Arret) erreurs = justes = 0;
    // Reprise d'une pause survenue pendant une attente : on attend toujours le même accord
    etat_ = nbRequis ? Etat::AttenteJoueur : Etat::Lecture;
    debutAttente = millis();
    dernierMicros = micros();
    bandeau::afficherNotesJouees(false);
    affichageModifie = true;
}

void pause() {
    if (etat_ != Etat::Lecture && etat_ != Etat::AttenteJoueur) return;
    etat_ = Etat::Pause;
    midiout::toutCouper();
    affichageModifie = true;
}

void stop() {
    if (etat_ == Etat::Vide) return;
    etat_ = Etat::Arret;
    positionner(0);
    bandeau::effacerLecture();
    bandeau::afficherNotesJouees(true);
}

void allerA(uint32_t ms) {
    if (etat_ == Etat::Vide) return;
    uint32_t us = ms * 1000;
    if (us > morceau.duree_us) us = morceau.duree_us;
    if (etat_ == Etat::AttenteJoueur) etat_ = Etat::Lecture;
    positionner(us);
}

void vitesse(uint8_t pourcent) { vitesse_ = constrain(pourcent, VITESSE_MIN, VITESSE_MAX); }

// Recalcule les notes affichées après un changement d'attribution ou de réglage
static void recalculer() {
    if (etat_ == Etat::Vide) return;
    if (etat_ == Etat::AttenteJoueur) etat_ = Etat::Lecture;
    positionner(positionUs());
}

void mode(Mode m) {
    if (m == mode_) return;
    mode_ = m;
    recalculer();
}

void travail(uint8_t t) {
    if (t > TRAVAIL_GAUCHE || t == travail_) return;
    travail_ = t;
    recalculer();
}

void attribuerMain(uint8_t groupe, uint8_t main) {
    if (groupe >= nbGroupes || main > MAIN_HAUTEUR) return;
    groupes[groupe].main = main;
    recalculer();
}

void partage(uint8_t note) {
    partage_ = note & 0x7F;
    recalculer();
}

void pointA() {
    if (etat_ == Etat::Vide) return;
    pointA_ = positionUs();
    if (pointB_ != UINT32_MAX && pointB_ <= pointA_) {
        pointB_ = UINT32_MAX;
        boucle_ = false;
    }
}

void pointB() {
    if (etat_ == Etat::Vide) return;
    if (pointA_ == UINT32_MAX) pointA_ = 0;
    const uint32_t pos = positionUs();
    if (pos <= pointA_) return;
    pointB_ = pos;
    boucle_ = true;
}

void effacerAB() {
    pointA_ = pointB_ = UINT32_MAX;
    boucle_ = false;
}

void boucleAB(bool actif) { boucle_ = actif && pointB_ != UINT32_MAX; }

void reglagesModifies() { recalculer(); }

Etat etat() { return etat_; }

const char* nomEtat() {
    switch (etat_) {
        case Etat::Vide: return "vide";
        case Etat::Arret: return "arret";
        case Etat::Lecture: return "lecture";
        case Etat::Pause: return "pause";
        case Etat::AttenteJoueur: return "attente";
    }
    return "?";
}

// ---------- JSON ----------

void etatJson(String& sortie) {
    JsonDocument d;
    d["t"] = "etat";
    d["e"] = nomEtat();
    d["m"] = mode_ == Mode::Demo ? "demo" : "attente";
    d["tr"] = travail_;
    d["f"] = nomFichier;
    d["pos"] = positionUs() / 1000;
    d["dur"] = morceau.duree_us / 1000;
    d["v"] = vitesse_;
    if (etat_ != Etat::Vide) {
        const float bpm = 60e6f / morceau.tempoA(positionUs()) * vitesse_ / 100.0f;
        d["bpm"] = roundf(bpm * 10) / 10;
    }
    if (pointA_ != UINT32_MAX) d["a"] = pointA_ / 1000;
    if (pointB_ != UINT32_MAX) d["b"] = pointB_ / 1000;
    d["bo"] = boucle_;
    d["err"] = erreurs;
    d["ok"] = justes;
    JsonArray notes = d["notes"].to<JsonArray>();
    for (int n = 0; n < 128 && notes.size() < 24; n++)
        if (actifs[n]) notes.add(n);
    JsonArray req = d["req"].to<JsonArray>();
    for (uint8_t i = 0; i < nbRequis; i++) req.add(requis[i]);
    serializeJson(d, sortie);
}

void morceauJson(String& sortie) {
    JsonDocument d;
    d["t"] = "morceau";
    d["f"] = nomFichier;
    d["dur"] = morceau.duree_us / 1000;
    d["partage"] = partage_;
    d["format"] = morceau.format;
    JsonArray gs = d["groupes"].to<JsonArray>();
    for (uint8_t i = 0; i < nbGroupes; i++) {
        const Groupe& g = groupes[i];
        JsonObject o = gs.add<JsonObject>();
        o["id"] = i;
        o["nom"] = g.nom;
        o["canal"] = g.canal + 1;
        o["n"] = g.nb_notes;
        o["min"] = g.min;
        o["max"] = g.max;
        o["moy"] = g.moy;
        o["main"] = g.main;
    }
    serializeJson(d, sortie);
}

}  // namespace moteur
