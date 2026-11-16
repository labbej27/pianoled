#!/usr/bin/env python3
"""Génère les fichiers MIDI de test de l'analyseur (test/fichiers/*.mid).

Python standard uniquement. Usage : python3 outils/generer_midi_test.py
Chaque fichier vise un cas précis ; les valeurs attendues sont vérifiées dans test/test_lecteur_smf.
"""
import os
import struct

DOSSIER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "test", "fichiers")


def vlq(n):
    """Quantité de longueur variable MIDI."""
    octets = [n & 0x7F]
    n >>= 7
    while n:
        octets.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(octets))


def piste(evenements, fin=True):
    """evenements : liste de (delta, octets bruts)."""
    corps = b"".join(vlq(d) + e for d, e in evenements)
    if fin:
        corps += vlq(0) + b"\xFF\x2F\x00"
    return b"MTrk" + struct.pack(">I", len(corps)) + corps


def entete(fmt, nb_pistes, division):
    return b"MThd" + struct.pack(">IHHH", 6, fmt, nb_pistes, division)


def tempo(us):
    return b"\xFF\x51\x03" + us.to_bytes(3, "big")


def nom(texte, type_=0x03):
    t = texte.encode("latin-1")
    return bytes([0xFF, type_]) + vlq(len(t)) + t


def ecrire(nom_fichier, donnees):
    with open(os.path.join(DOSSIER, nom_fichier), "wb") as f:
        f.write(donnees)
    print(f"  {nom_fichier} ({len(donnees)} octets)")


def main():
    os.makedirs(DOSSIER, exist_ok=True)
    print("Génération dans", os.path.normpath(DOSSIER))

    # 1. Format 0 : gamme do-ré-mi-fa, une noire chacune, 480 ticks/noire, 120 BPM (500 000 µs/noire)
    ev = [(0, tempo(500000))]
    for n in (60, 62, 64, 65):
        ev += [(0, bytes([0x90, n, 100])), (480, bytes([0x80, n, 0]))]
    ecrire("format0_gamme.mid", entete(0, 1, 480) + piste(ev))

    # 2. Running status + Note On de vélocité 0 comme relâchement, delta sur 2 octets (200 ticks)
    ev = [(0, bytes([0x90, 60, 80])), (200, bytes([60, 0])), (0, bytes([62, 90])), (200, bytes([62, 0]))]
    ecrire("running_status.mid", entete(0, 1, 100) + piste(ev))

    # 3. Format 1 : piste 0 = tempos (120 BPM puis 60 BPM à 960 ticks), piste 1 = main droite, piste 2 = main gauche
    p0 = piste([(0, nom("Conducteur")), (0, tempo(500000)), (960, tempo(1000000))])
    md = [(0, nom("Main droite"))]
    for n in (72, 74, 76, 77):  # une noire chacune, de 0 à 1920 ticks
        md += [(0, bytes([0x90, n, 100])), (480, bytes([0x80, n, 64]))]
    p1 = piste(md)
    mg = [(0, nom("Piano", 0x04)), (0, bytes([0x91, 48, 70])), (960, bytes([0x81, 48, 0])),
          (0, bytes([0x91, 43, 70])), (960, bytes([0x81, 43, 0]))]
    p2 = piste(mg)
    ecrire("format1_deux_mains.mid", entete(1, 3, 480) + p0 + p1 + p2)

    # 4. SysEx, méta-événements divers, programme (1 octet de donnée), bloc inconnu entre les pistes
    ev = [(0, b"\xF0" + vlq(5) + b"\x7E\x7F\x09\x01\xF7"),  # GM System On
          (0, b"\xFF\x01\x05Texte"),                         # méta texte
          (0, bytes([0xC0, 0])),                             # changement de programme
          (0, bytes([0xB0, 7, 100])),                        # contrôleur
          (0, bytes([0x90, 67, 100])),
          (10, b"\xF7" + vlq(2) + b"\x01\x02"),              # SysEx « échappé »
          (10, bytes([0x80, 67, 0]))]
    inconnu = b"XFIH" + struct.pack(">I", 4) + b"\x00\x01\x02\x03"
    ecrire("evenements_ignores.mid", entete(0, 1, 96) + inconnu + piste(ev))

    # 5. Division SMPTE : 25 images/s × 40 ticks/image = 1 000 ticks/s → 1 tick = 1 ms
    division = ((256 - 25) << 8) | 40
    ev = [(0, bytes([0x90, 60, 100])), (500, bytes([0x80, 60, 0]))]
    ecrire("smpte.mid", entete(0, 1, division) + piste(ev))

    # 6. Fichier tronqué au milieu d'une piste
    ev = [(0, bytes([0x90, 60, 100])), (100, bytes([0x80, 60, 0])), (0, bytes([0x90, 62, 100])),
          (100, bytes([0x80, 62, 0]))]
    complet = entete(0, 1, 96) + piste(ev)
    ecrire("tronque.mid", complet[:-9])

    # 7. Format 2 (non géré)
    ecrire("format2.mid", entete(2, 1, 96) + piste([(0, bytes([0x90, 60, 100])), (10, bytes([0x80, 60, 0]))]))

    # 8. Pas un fichier MIDI
    ecrire("pas_midi.mid", b"RIFF\x00\x00\x00\x00WAVEfmt ")

    # 9. Accord + note répétée au même instant (relâchement puis réappui de la même note)
    ev = [(0, bytes([0x90, 60, 100])), (0, bytes([0x90, 64, 100])), (0, bytes([0x90, 67, 100])),
          (480, bytes([0x80, 60, 0])), (0, bytes([0x90, 60, 100])),  # do répété
          (0, bytes([0x80, 64, 0])), (0, bytes([0x80, 67, 0])),
          (480, bytes([0x80, 60, 0]))]
    ecrire("accord_repetition.mid", entete(0, 1, 480) + piste(ev))


if __name__ == "__main__":
    main()
