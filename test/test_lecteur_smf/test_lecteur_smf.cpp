// Tests unitaires de l'analyseur SMF (environnement PlatformIO « native » + Unity)
// Les fichiers sont produits par outils/generer_midi_test.py.
#include <stdio.h>
#include <string.h>
#include <unity.h>
#include <string>
#include <vector>
#include "lecteur_smf.h"

using namespace smf;

#ifndef DOSSIER_FICHIERS_TEST
#define DOSSIER_FICHIERS_TEST "test/fichiers"
#endif

// Source sur un fichier du disque
class SourceFichier : public Source {
public:
    explicit SourceFichier(const char* nom) {
        std::string chemin = std::string(DOSSIER_FICHIERS_TEST) + "/" + nom;
        FILE* f = fopen(chemin.c_str(), "rb");
        if (!f) return;
        uint8_t tampon[4096];
        size_t n;
        while ((n = fread(tampon, 1, sizeof tampon, f)) > 0) d_.insert(d_.end(), tampon, tampon + n);
        fclose(f);
        ouvert_ = true;
    }
    bool ouvert() const { return ouvert_; }
    uint32_t taille() const override { return (uint32_t)d_.size(); }
    bool lire(uint32_t pos, uint8_t* dst, uint32_t n) override {
        if (pos > d_.size() || n > d_.size() - pos) return false;
        memcpy(dst, d_.data() + pos, n);
        return true;
    }

private:
    std::vector<uint8_t> d_;
    bool ouvert_ = false;
};

static Erreur charger(const char* nom, Morceau& m, uint32_t max = MAX_EVENEMENTS_DEFAUT) {
    SourceFichier s(nom);
    if (!s.ouvert()) {
        char msg[200];
        snprintf(msg, sizeof msg, "fichier de test introuvable : %s (lancer outils/generer_midi_test.py)", nom);
        TEST_FAIL_MESSAGE(msg);
    }
    return analyser(s, m, max);
}

static void verifier(const Evenement& e, uint32_t t, uint8_t note, uint8_t vel, uint8_t piste, uint8_t canal) {
    TEST_ASSERT_EQUAL_UINT32(t, e.temps_us);
    TEST_ASSERT_EQUAL_UINT8(note, e.note);
    TEST_ASSERT_EQUAL_UINT8(vel, e.velocite);
    TEST_ASSERT_EQUAL_UINT8(piste, e.piste);
    TEST_ASSERT_EQUAL_UINT8(canal, e.canal);
}

void test_format0_gamme() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::Aucune, charger("format0_gamme.mid", m));
    TEST_ASSERT_EQUAL_UINT16(0, m.format);
    TEST_ASSERT_EQUAL_UINT16(480, m.division);
    TEST_ASSERT_EQUAL_UINT8(1, m.nb_pistes);
    TEST_ASSERT_EQUAL_UINT32(8, m.nb_evenements);
    // 120 BPM : une noire = 500 ms
    verifier(m.evenements[0], 0, 60, 100, 0, 0);
    verifier(m.evenements[1], 500000, 60, 0, 0, 0);  // relâchement avant l'appui suivant
    verifier(m.evenements[2], 500000, 62, 100, 0, 0);
    verifier(m.evenements[7], 2000000, 65, 0, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(2000000, m.duree_us);
    TEST_ASSERT_EQUAL_UINT32(4, m.pistes[0].nb_notes);
    TEST_ASSERT_EQUAL_UINT8(60, m.pistes[0].note_min);
    TEST_ASSERT_EQUAL_UINT8(65, m.pistes[0].note_max);
}

void test_running_status_et_velocite_nulle() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::Aucune, charger("running_status.mid", m));
    TEST_ASSERT_EQUAL_UINT32(4, m.nb_evenements);
    // Pas de tempo : 120 BPM par défaut ; 100 ticks/noire → 200 ticks = 1 s (delta codé sur 2 octets)
    verifier(m.evenements[0], 0, 60, 80, 0, 0);
    verifier(m.evenements[1], 1000000, 60, 0, 0, 0);
    verifier(m.evenements[2], 1000000, 62, 90, 0, 0);
    verifier(m.evenements[3], 2000000, 62, 0, 0, 0);
}

void test_format1_fusion_et_changement_de_tempo() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::Aucune, charger("format1_deux_mains.mid", m));
    TEST_ASSERT_EQUAL_UINT16(1, m.format);
    TEST_ASSERT_EQUAL_UINT8(3, m.nb_pistes);
    TEST_ASSERT_EQUAL_STRING("Conducteur", m.pistes[0].nom);
    TEST_ASSERT_EQUAL_STRING("Main droite", m.pistes[1].nom);
    TEST_ASSERT_EQUAL_STRING("Piano", m.pistes[2].nom);  // nom d'instrument faute de nom de piste
    TEST_ASSERT_EQUAL_UINT32(0, m.pistes[0].nb_notes);
    TEST_ASSERT_EQUAL_UINT32(4, m.pistes[1].nb_notes);
    TEST_ASSERT_EQUAL_UINT32(2, m.pistes[2].nb_notes);
    TEST_ASSERT_EQUAL_UINT16(1u << 1, m.pistes[2].masque_canaux);

    // Tempos : 500 000 µs/noire jusqu'à 960 ticks (= 1 s), puis 1 000 000 µs/noire
    TEST_ASSERT_EQUAL_UINT16(2, m.nb_tempos);
    TEST_ASSERT_EQUAL_UINT32(1000000, m.tempos[1].temps_us);
    TEST_ASSERT_EQUAL_UINT32(500000, m.tempoA(999999));
    TEST_ASSERT_EQUAL_UINT32(1000000, m.tempoA(1000000));

    TEST_ASSERT_EQUAL_UINT32(12, m.nb_evenements);
    // Instant 0 : do3 (main gauche, canal 2) et do5 (main droite), triés par piste
    verifier(m.evenements[0], 0, 72, 100, 1, 0);
    verifier(m.evenements[1], 0, 48, 70, 2, 1);
    // 480 ticks = 0,5 s
    verifier(m.evenements[2], 500000, 72, 0, 1, 0);
    verifier(m.evenements[3], 500000, 74, 100, 1, 0);
    // 960 ticks = 1 s : changement de tempo ; relâchements (pistes 1 puis 2) puis appuis
    verifier(m.evenements[4], 1000000, 74, 0, 1, 0);
    verifier(m.evenements[5], 1000000, 48, 0, 2, 1);
    verifier(m.evenements[6], 1000000, 76, 100, 1, 0);
    verifier(m.evenements[7], 1000000, 43, 70, 2, 1);
    // 1440 ticks : 1 s + 480 ticks à 60 BPM (1 s par noire) = 2 s
    verifier(m.evenements[8], 2000000, 76, 0, 1, 0);
    verifier(m.evenements[9], 2000000, 77, 100, 1, 0);
    // 1920 ticks = 3 s
    verifier(m.evenements[10], 3000000, 77, 0, 1, 0);
    verifier(m.evenements[11], 3000000, 43, 0, 2, 1);
    TEST_ASSERT_EQUAL_UINT32(3000000, m.duree_us);
}

void test_sysex_meta_et_bloc_inconnu_ignores() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::Aucune, charger("evenements_ignores.mid", m));
    TEST_ASSERT_EQUAL_UINT32(2, m.nb_evenements);
    // 96 ticks/noire à 120 BPM : 20 ticks = 104 166 µs
    verifier(m.evenements[0], 0, 67, 100, 0, 0);
    verifier(m.evenements[1], 104166, 67, 0, 0, 0);
}

void test_division_smpte() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::Aucune, charger("smpte.mid", m));
    TEST_ASSERT_EQUAL_UINT32(2, m.nb_evenements);
    TEST_ASSERT_EQUAL_UINT32(500000, m.evenements[1].temps_us);  // 500 ticks de 1 ms
}

void test_fichier_tronque_lu_partiellement() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::Aucune, charger("tronque.mid", m));
    TEST_ASSERT_TRUE(m.nb_evenements >= 2);  // au moins la première note complète
    verifier(m.evenements[0], 0, 60, 100, 0, 0);
}

void test_format2_refuse() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::FormatNonGere, charger("format2.mid", m));
    TEST_ASSERT_NULL(m.evenements);
}

void test_pas_un_fichier_midi() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::PasUnFichierMidi, charger("pas_midi.mid", m));
}

void test_limite_d_evenements() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::TropDEvenements, charger("format0_gamme.mid", m, 7));
    TEST_ASSERT_NULL(m.evenements);
}

void test_accord_et_note_repetee() {
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::Aucune, charger("accord_repetition.mid", m));
    TEST_ASSERT_EQUAL_UINT32(8, m.nb_evenements);
    verifier(m.evenements[0], 0, 60, 100, 0, 0);
    verifier(m.evenements[1], 0, 64, 100, 0, 0);
    verifier(m.evenements[2], 0, 67, 100, 0, 0);
    // À 0,5 s : les trois relâchements passent avant le réappui du do
    verifier(m.evenements[3], 500000, 60, 0, 0, 0);
    verifier(m.evenements[4], 500000, 64, 0, 0, 0);
    verifier(m.evenements[5], 500000, 67, 0, 0, 0);
    verifier(m.evenements[6], 500000, 60, 100, 0, 0);
    verifier(m.evenements[7], 1000000, 60, 0, 0, 0);
}

void test_source_memoire_et_entete_vide() {
    static const uint8_t vide[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 0, 0, 96};
    SourceMemoire s(vide, sizeof vide);
    Morceau m;
    TEST_ASSERT_EQUAL(Erreur::AucuneNote, analyser(s, m));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_format0_gamme);
    RUN_TEST(test_running_status_et_velocite_nulle);
    RUN_TEST(test_format1_fusion_et_changement_de_tempo);
    RUN_TEST(test_sysex_meta_et_bloc_inconnu_ignores);
    RUN_TEST(test_division_smpte);
    RUN_TEST(test_fichier_tronque_lu_partiellement);
    RUN_TEST(test_format2_refuse);
    RUN_TEST(test_pas_un_fichier_midi);
    RUN_TEST(test_limite_d_evenements);
    RUN_TEST(test_accord_et_note_repetee);
    RUN_TEST(test_source_memoire_et_entete_vide);
    return UNITY_END();
}
