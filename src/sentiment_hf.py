#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Classificação de sentimento (Positiva/Neutra/Negativa) usando Hugging Face Transformers no WSL.
Lê data/songs.tsv (artist \t text) e salva as contagens em out/sentiment_counts.csv.

Uso (CPU):
  source .venv/bin/activate   # se estiver usando venv
  python3 src/sentiment_hf.py --max 2000 --out out/sentiment_counts.csv

Parâmetros úteis:
  --model   nome do modelo no HF Hub (default: lxyuan/distilbert-base-multilingual-cased-sentiments-student)
  --max     máximo de músicas a processar (p/ testes rápidos)
  --skip    pula N músicas do começo (p/ fatiar em rodadas)
  --batch   tamanho do lote (padrão 8) - aumenta para acelerar (se tiver RAM/GPU)
  --device  "cpu" | "cuda" (auto-detect se não informado)
"""

import argparse
from pathlib import Path
from tqdm import tqdm
from transformers import pipeline, AutoTokenizer, AutoModelForSequenceClassification
import torch
import re

# Caminho do arquivo com as músicas (pré-processado pelo preprocess_csv.py)
DATA = Path("data/songs.tsv")

# ---------------------------------------------------------------
# Funções auxiliares
# ---------------------------------------------------------------

def detect_device(arg_device: str | None) -> str:
    """Se usuário passou --device, respeita; caso contrário, usa cuda se disponível."""
    if arg_device:
        return arg_device
    return "cuda" if torch.cuda.is_available() else "cpu"


def load_pipeline(model_name: str, device: str, batch_size: int):
    """
    Cria pipeline de análise de sentimento do HF:
    - use_fast=False: evita dependências extras (ex.: tiktoken) em alguns tokenizers.
    - truncation+max_length=512: limita tamanho p/ estabilidade em CPU/GPU.
    """
    tok = AutoTokenizer.from_pretrained(model_name, use_fast=False)
    mdl = AutoModelForSequenceClassification.from_pretrained(model_name)
    pipe = pipeline(
        "sentiment-analysis",
        model=mdl,
        tokenizer=tok,
        device=0 if (device == "cuda") else -1,  # -1 = CPU
        truncation=True,
        max_length=512,
        batch_size=batch_size
    )
    return pipe


def _pick_label(pred):
    """
    Extrai a 'label' principal independente do formato:
      - dict {'label': 'POSITIVE', 'score': ...}
      - list [{'label': 'NEGATIVE','score':...}, ...] → pega a de maior score
    """
    if isinstance(pred, dict) and 'label' in pred:
        return pred['label']
    if isinstance(pred, list) and pred and isinstance(pred[0], dict):
        best = max(pred, key=lambda d: d.get('score', 0.0))
        return best.get('label', 'NEUTRAL')
    return 'NEUTRAL'


def _normalize_label_to_pt(label_raw: str) -> str:
    """
    Normaliza rótulos de diferentes modelos para {Positiva, Neutra, Negativa}.
    Suporta também modelos de 5 estrelas (nlptown), mapeando:
      <=2 → Negativa, 3 → Neutra, >=4 → Positiva.
    """
    s = str(label_raw).strip().lower()

    base_map = {
        'positive': 'Positiva',
        'pos': 'Positiva',
        'neutral': 'Neutra',
        'neu': 'Neutra',
        'negative': 'Negativa',
        'neg': 'Negativa',
        'label_0': 'Negativa', // alguns modelos indexados
        'label_1': 'Neutra',
        'label_2': 'Positiva',
    }
    if s in base_map:
        return base_map[s]

    # Modelos baseados em "stars"
    if 'star' in s:
        m = re.search(r'(\d+)', s)
        if m:
            n = int(m.group(1))
            if n <= 2:
                return 'Negativa'
            elif n == 3:
                return 'Neutra'
            else:
                return 'Positiva'

    return 'Neutra'


def batch_iter(lines, batch):
    """Agrupa itens de um iterável em lotes de tamanho 'batch'."""
    buf = []
    for x in lines:
        buf.append(x)
        if len(buf) >= batch:
            yield buf
            buf = []
    if buf:
        yield buf


# ---------------------------------------------------------------
# Programa principal
# ---------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="lxyuan/distilbert-base-multilingual-cased-sentiments-student")
    ap.add_argument("--max", type=int, default=500)
    ap.add_argument("--skip", type=int, default=0)
    ap.add_argument("--out", default="out/sentiment_counts.csv")
    ap.add_argument("--batch", type=int, default=8)
    ap.add_argument("--device", choices=["cpu","cuda"], default=None)
    args = ap.parse_args()

    device = detect_device(args.device)
    outp = Path(args.out)
    outp.parent.mkdir(parents=True, exist_ok=True)

    # Carrega pipeline uma vez (modelo + tokenizer) e reaproveita durante todo o loop
    pipe = load_pipeline(args.model, device, args.batch)

    counts = {"Positiva":0, "Neutra":0, "Negativa":0}
    processed = 0
    seen = 0

    # Gerador que lê 'data/songs.tsv' e rende somente o campo 'text'
    def read_texts():
        nonlocal seen, processed
        with DATA.open("r", encoding="utf-8") as fin:
            for line in fin:
                if args.max and processed >= args.max:
                    break
                if args.skip and seen < args.skip:
                    seen += 1
                    continue
                seen += 1
                tab = line.find("\t")
                if tab < 0:
                    continue
                text = line[tab+1:].strip()
                if not text:
                    continue
                yield text

    # Loop principal: lê em lotes → chama pipeline → contabiliza classes
    for texts in tqdm(batch_iter(read_texts(), args.batch), desc=f"sentiment[{device}]"):
        res = pipe(texts)
        for r in res:
            label = _normalize_label_to_pt(_pick_label(r))
            counts[label] += 1
            processed += 1

    # Persistimos as contagens finais para integração com BI/relatório
    with outp.open("w", encoding="utf-8") as fo:
        fo.write("class;count\n")
        for k in ("Positiva","Neutra","Negativa"):
            fo.write(f"{k};{counts[k]}\n")

    print(f"[sentiment-hf] model={args.model} device={device} processed={processed} → {outp}")
    print(counts)


if __name__ == "__main__":
    main()
