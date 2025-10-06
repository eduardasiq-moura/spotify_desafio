#!/usr/bin/env bash
# Coleta métricas e gera a "base" em out/
# - Wordcount e Artistcount: P em [1,2,4,6,12] com --oversubscribe
# - Sentiment HF: batches [4,8,16] com --max 500 (ajuste se quiser)
# - Top-100 de palavras e artistas em out/
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p out

# ---------- helpers ----------
extract_ttotal () {
  # Extrai t_total=XX.XXXs da saída dos binários C
  awk -F't_total=' '/t_total/ { gsub("s","",$2); print $2 }'
}

measure_sentiment () {
  local batch="$1"
  local maxn="${2:-500}"
  local outfile="out/sentiment_counts_b${batch}.csv"
  # mede com /usr/bin/time (tempo "real" em segundos)
  local tfile
  tfile="$(mktemp)"
  /usr/bin/time -f "%e" python3 src/sentiment_hf.py --batch "$batch" --device cpu --max "$maxn" --out "$outfile" 1>/dev/null 2> "$tfile" || true
  cat "$tfile"
  rm -f "$tfile"
}

# ---------- sanity ----------
if [ ! -f data/songs.tsv ]; then
  echo "[!] data/songs.tsv não encontrado. Gerando com preprocess_csv.py..."
  python3 src/preprocess_csv.py
fi

if [ ! -x build/mpi_wordcount ] || [ ! -x build/mpi_artistcount ]; then
  echo "[!] Binários MPI não encontrados. Compilando..."
  make
fi

# ---------- WORDCOUNT ----------
echo "P,Tempo(s)" > out/tempos_wordcount.csv
for p in 1 2 4 6 12; do
  echo "[wordcount] P=$p"
  tempo=$(mpirun --oversubscribe -np "$p" ./build/mpi_wordcount | extract_ttotal)
  echo "$p,$tempo" >> out/tempos_wordcount.csv
done

# Garante que out/wordcount.csv exista (o binário já o gera)
if [ -f out/wordcount.csv ]; then
  LC_ALL=C sort -t';' -k2,2nr out/wordcount.csv | head -n 101 > out/top_words.csv
else
  echo "[WARN] out/wordcount.csv não encontrado (rode ao menos 1x o mpi_wordcount)."
fi

# ---------- ARTISTCOUNT ----------
echo "P,Tempo(s)" > out/tempos_artistcount.csv
for p in 1 2 4 6 12; do
  echo "[artistcount] P=$p"
  tempo=$(mpirun --oversubscribe -np "$p" ./build/mpi_artistcount | extract_ttotal)
  echo "$p,$tempo" >> out/tempos_artistcount.csv
done

# Garante que out/artists.csv exista (o binário já o gera)
if [ -f out/artists.csv ]; then
  LC_ALL=C sort -t';' -k2,2nr out/artists.csv | head -n 101 > out/top_artists.csv
else
  echo "[WARN] out/artists.csv não encontrado (rode ao menos 1x o mpi_artistcount)."
fi

# ---------- SENTIMENT ----------
echo "Batch,MaxSamples,Tempo(s)" > out/tempos_sentiment.csv
for b in 4 8 16; do
  echo "[sentiment] batch=$b"
  stime=$(measure_sentiment "$b" 500)
  echo "$b,500,$stime" >> out/tempos_sentiment.csv
done

echo "[OK] Base pronta em out/:"
echo "  - tempos_wordcount.csv, tempos_artistcount.csv, tempos_sentiment.csv"
echo "  - wordcount.csv, artists.csv"
echo "  - top_words.csv, top_artists.csv"
