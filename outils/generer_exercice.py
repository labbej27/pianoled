#!/usr/bin/env python3
"""Génère un exercice à deux mains pour essayer les modes démo et attente.

Format 1, 3 pistes : tempo, « Main droite » (gamme de do montante puis descendante),
« Main gauche » (accords do / fa / sol / do, une ronde chacun). 90 BPM, 480 ticks par noire.
Usage : python3 outils/generer_exercice.py [fichier_de_sortie.mid]
"""
import os
import struct
import sys

from generer_midi_test import entete, nom, piste, tempo

NOIRE = 480


def main():
    sortie = sys.argv[1] if len(sys.argv) > 1 else "exercice_deux_mains.mid"
    conducteur = piste([(0, nom("Exercice deux mains")), (0, tempo(60_000_000 // 90))])

    gamme = [60, 62, 64, 65, 67, 69, 71, 72, 72, 71, 69, 67, 65, 64, 62, 60]
    md = [(0, nom("Main droite"))]
    for n in gamme:  # une noire par note
        md += [(0, bytes([0x90, n, 90])), (NOIRE, bytes([0x80, n, 0]))]

    accords = [(48, 52, 55), (41, 45, 48), (43, 47, 50), (48, 52, 55)]  # do, fa, sol, do
    mg = [(0, nom("Main gauche"))]
    for accord in accords:  # une ronde par accord
        mg += [(0, bytes([0x91, n, 70])) for n in accord]
        mg += [(NOIRE * 4 if i == 0 else 0, bytes([0x81, n, 0])) for i, n in enumerate(accord)]

    donnees = entete(1, 3, NOIRE) + conducteur + piste(md) + piste(mg)
    with open(sortie, "wb") as f:
        f.write(donnees)
    print(f"{sortie} : {len(donnees)} octets")


if __name__ == "__main__":
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    main()
