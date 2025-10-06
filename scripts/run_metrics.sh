#!/usr/bin/env bash
# Executa todos os testes de desempenho (Wordcount, Artistcount, Sentiment)
# Gera CSVs e dashboard em out/, e sobe HTTP local.
# ------------------------------------------
set -Euo pipefail
cd "$(dirname "$0")/.."
mkdir -p out

# Mostra erros mas continua o script
trap 'echo "[ERRO] Linha $LINENO falhou, continuando..."' ERR

# ---------- helpers ----------
extract_ttotal () {
  awk -F't_total=' '/t_total/ { gsub("s","",$2); print $2 }'
}

measure_sentiment () {
  local batch="$1"
  local maxn="${2:-500}"
  local outfile="out/sentiment_counts_b${batch}.csv"
  local tfile="$(mktemp)"
  echo "[sentiment] batch=$batch"
  # executa e ignora erros leves
  /usr/bin/time -f "%e" python3 src/sentiment_hf.py \
    --batch "$batch" --device cpu --max "$maxn" --out "$outfile" \
    1>/dev/null 2> "$tfile" || true
  cat "$tfile"
  rm -f "$tfile"
}

# ---------- sanity ----------
if [ ! -f data/songs.tsv ]; then
  echo "[!] data/songs.tsv não encontrado. Gerando..."
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
  tempo=$(mpirun --oversubscribe -np "$p" ./build/mpi_wordcount 2>/dev/null | extract_ttotal || true)
  echo "$p,${tempo:-0}" >> out/tempos_wordcount.csv
done
[ -f out/wordcount.csv ] && LC_ALL=C sort -t';' -k2,2nr out/wordcount.csv | head -n 101 > out/top_words.csv

# ---------- ARTISTCOUNT ----------
echo "P,Tempo(s)" > out/tempos_artistcount.csv
for p in 1 2 4 6 12; do
  echo "[artistcount] P=$p"
  tempo=$(mpirun --oversubscribe -np "$p" ./build/mpi_artistcount 2>/dev/null | extract_ttotal || true)
  echo "$p,${tempo:-0}" >> out/tempos_artistcount.csv
done
if [ -f out/artists.csv ]; then
  LC_ALL=C sort -t';' -k2,2nr out/artists.csv | head -n 101 > out/top_artists.csv
elif [ -f out/artistcount.csv ]; then
  LC_ALL=C sort -t';' -k2,2nr out/artistcount.csv | head -n 101 > out/top_artists.csv
else
  echo "[WARN] Nenhum arquivo de artistas encontrado."
fi

# ---------- SENTIMENT ----------
echo "Batch,MaxSamples,Tempo(s)" > out/tempos_sentiment.csv
for b in 4 8 16; do
  stime=$(measure_sentiment "$b" 500)
  echo "$b,500,${stime:-0}" >> out/tempos_sentiment.csv
done

# ---------- DASHBOARD ----------
echo "[Dashboard] Gerando HTML..."
python3 tools/make_dashboard.py || echo "[WARN] Não foi possível gerar o dashboard."

# ---------- HTTP ----------
PORT=8000
echo ""
echo "----------------------------------------------------------"
echo "✅ Concluído! Acesse: http://localhost:${PORT}/out/dashboard.html"
echo "----------------------------------------------------------"
python3 -m http.server "$PORT"
