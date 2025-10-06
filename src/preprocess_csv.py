# -*- coding: utf-8 -*-
"""
Pré-processamento do CSV do Kaggle → TSV seguro.

- Lê o CSV original (com aspas, vírgulas e quebras de linha internas).
- Extrai apenas 'artist' e 'text'.
- Gera 'data/songs.tsv' com cada música em uma linha no formato: artist \t text
  * substituímos qualquer TAB no texto por espaço, para não quebrar o TSV.
"""

import csv
from pathlib import Path

IN = Path("data/spotify_millsongdata.csv")
OUT = Path("data/songs.tsv")

def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    # newline="" evita transformar \r\n e mantém leitura correta do csv
    with IN.open("r", encoding="utf-8", newline="") as fin, \
         OUT.open("w", encoding="utf-8", newline="") as fout:
        reader = csv.DictReader(fin)
        # Espera colunas: artist, song, link, text (dataset Kaggle)
        # Vamos usar apenas 'artist' e 'text'
        total = 0
        for row in reader:
            artist = (row.get("artist") or "").strip()
            # Se houver TABs no texto, trocamos por espaço para não quebrar o TSV
            text = (row.get("text") or "").replace("\t", " ").replace("\r", " ").replace("\n", " ").strip()
            # Linha TSV: artist \t text \n
            fout.write(f"{artist}\t{text}\n")
            total += 1
    print(f"OK: {total} registros → {OUT}")

if __name__ == "__main__":
    main()
