#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Classificação de sentimento (Positiva/Neutra/Negativa) usando Hugging Face Transformers no WSL.
Lê data/songs.tsv (artist \t text) e salva as contagens em out/sentiment_counts.csv.

Uso (CPU):
  source .venv/bin/activate   # se estiver usando venv
  python3 src/sentiment_hf.py --max 2000 --out out/sentiment_counts.csv

Parâmetros úteis:
  --model   nome do modelo no HF Hub (default: cardiffnlp/twitter-xlm-roberta-base-sentiment)
  --max     máximo de músicas a processar (p/ testes rápidos)
  --skip    pula N músicas do começo (p/ fatiar em rodadas)
  --batch   tamanho do lote (padrão 8) - aumenta para acelerar (se tiver RAM/GPU)
  --device  "cpu" | "cuda" (auto-detect se não informado)
"""

import argparse
from pathlib import Path
from typing import List
from tqdm import tqdm

from transformers import pipeline, AutoTokenizer, AutoModelForSequenceClassification
import torch

DATA = Path("data/songs.tsv")

# Mapeamento de rótulos -> nossas classes finais
LABEL_MAP = {
    "NEGATIVE": "Negativa",
    "NEUTRAL":  "Neutra",
    "POSITIVE": "Positiva",
    # Alguns modelos usam outros rótulos
    "LABEL_0":  "Negativa",
    "LABEL_1":  "Neutra",
    "LABEL_2":  "Positiva",
    "NEG":      "Negativa",
    "NEU":      "Neutra",
    "POS":      "Positiva",
}

def detect_device(arg_device: str | None) -> str:
    if arg_device:
        return arg_device
    return "cuda" if torch.cuda.is_available() else "cpu"

def load_pipeline(model_name: str, device: str, batch_size: int):
    tok = AutoTokenizer.from_pretrained(model_name)
    mdl = AutoModelForSequenceClassification.from_pretrained(model_name)
    pipe = pipeline(
        "sentiment-analysis",
        model=mdl,
        tokenizer=tok,
        device=0 if (device == "cuda") else -1,
        truncation=True,
        max_length=512,
        batch_size=batch_size,
        top_k=None
    )
    return pipe

def normalize_label(lbl: str) -> str:
    lbl = lbl.strip().upper()
    return LABEL_MAP.get(lbl, "Neutra")

def batch_iter(lines, batch):
    buf = []
    for x in lines:
        buf.append(x)
        if len(buf) >= batch:
            yield buf
            buf = []
    if buf:
        yield buf

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="cardiffnlp/twitter-xlm-roberta-base-sentiment")
    ap.add_argument("--max", type=int, default=500)
    ap.add_argument("--skip", type=int, default=0)
    ap.add_argument("--out", default="out/sentiment_counts.csv")
    ap.add_argument("--batch", type=int, default=8)
    ap.add_argument("--device", choices=["cpu","cuda"], default=None)
    args = ap.parse_args()

    device = detect_device(args.device)
    outp = Path(args.out)
    outp.parent.mkdir(parents=True, exist_ok=True)

    pipe = load_pipeline(args.model, device, args.batch)

    counts = {"Positiva":0, "Neutra":0, "Negativa":0}
    processed = 0
    seen = 0

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

    for texts in tqdm(batch_iter(read_texts(), args.batch), desc=f"sentiment[{device}]"):
        res = pipe(texts)
        for r in res:
            label = normalize_label(str(r.get("label","NEUTRAL")))
            counts[label] += 1
            processed += 1

    with outp.open("w", encoding="utf-8") as fo:
        fo.write("class;count\n")
        for k in ("Positiva","Neutra","Negativa"):
            fo.write(f"{k};{counts[k]}\n")

    print(f"[sentiment-hf] model={args.model} device={device} processed={processed} → {outp}")
    print(counts)

if __name__ == "__main__":
    main()
