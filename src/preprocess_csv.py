#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Preprocessa o CSV do Kaggle para TSV seguro (uma linha por música).
Saída: data/songs.tsv com colunas: artist \t text
"""

import csv
from pathlib import Path

IN = Path("data/spotify_millsongdata.csv")
OUT = Path("data/songs.tsv")

def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with IN.open("r", encoding="utf-8", newline="") as fin, \
         OUT.open("w", encoding="utf-8", newline="") as fout:
        reader = csv.DictReader(fin)
        # Espera colunas: artist, song, link, text (dataset Kaggle)
        # Vamos usar apenas artist e text
        total = 0
        for row in reader:
            artist = (row.get("artist") or "").strip()
            text = (row.get("text") or "").replace("\t", " ").strip()
            # Linha TSV: artist \t text \n
            fout.write(f"{artist}\t{text}\n")
            total += 1
    print(f"OK: {total} registros → {OUT}")

if __name__ == "__main__":
    main()
