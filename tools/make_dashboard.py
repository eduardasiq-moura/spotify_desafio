#!/usr/bin/env python3
# Gera um dashboard HTML (out/dashboard.html) a partir dos CSVs em out/.
import csv, json
from pathlib import Path

OUT = Path("out")
OUT.mkdir(parents=True, exist_ok=True)

WC = OUT/"tempos_wordcount.csv"
AC = OUT/"tempos_artistcount.csv"
SE = OUT/"tempos_sentiment.csv"
DASH = OUT/"dashboard.html"
INDEX = OUT/"index.html"

def load_csv_times(path: Path):
    xs, ys = [], []
    with path.open(encoding="utf-8") as f:
        rdr = csv.reader(f)
        next(rdr, None)  # header
        for row in rdr:
            if not row: continue
            xs.append(int(row[0]))
            ys.append(float(row[1]))
    return xs, ys

def compute_speedup_eff(xp, tp):
    if not tp: return [], [], 0.0
    T1 = tp[0]
    speedup = [T1/t if t>0 else 0 for t in tp]
    eff = [s/x if x>0 else 0 for s, x in zip(speedup, xp)]
    return speedup, eff, T1

def load_sentiment(path: Path):
    batches, maxs, times = [], [], []
    with path.open(encoding="utf-8") as f:
        rdr = csv.reader(f)
        next(rdr, None)
        for row in rdr:
            if not row: continue
            batches.append(int(row[0]))
            maxs.append(int(row[1]))
            times.append(float(row[2]))
    return batches, maxs, times

def html_page(wc_x, wc_t, wc_s, wc_e, ac_x, ac_t, ac_s, ac_e, se_b, se_t):
    return f"""<!doctype html>
<html lang="pt-br">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Dashboard de Métricas - Spotify MPI + HF</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
<style>
body {{ font-family: system-ui, -apple-system, "Segoe UI", Roboto, Arial, sans-serif; margin: 24px; }}
h1 {{ margin-bottom: 4px; }}
.card {{ border: 1px solid #eee; border-radius: 12px; padding: 16px; margin: 16px 0; box-shadow: 0 2px 6px rgba(0,0,0,.04);}}
.grid {{ display: grid; gap: 16px; grid-template-columns: repeat(auto-fit,minmax(320px,1fr)); }}
code, pre {{ background:#f7f7f7; padding:2px 6px; border-radius:6px;}}
.table {{ border-collapse: collapse; width: 100%; }}
.table th,.table td {{ border:1px solid #ddd; padding:8px; text-align:right;}}
.table th:first-child,.table td:first-child {{ text-align:left;}}
small {{ color:#666; }}
a.button {{ display:inline-block; padding:8px 12px; border:1px solid #ddd; border-radius:8px; text-decoration:none; }}
</style>
</head>
<body>
<h1>Dashboard de Métricas</h1>
<small>Dados lidos de <code>out/</code>.</small>
<p>
  <a class="button" href="tempos_wordcount.csv">tempos_wordcount.csv</a>
  <a class="button" href="tempos_artistcount.csv">tempos_artistcount.csv</a>
  <a class="button" href="tempos_sentiment.csv">tempos_sentiment.csv</a>
  <a class="button" href="top_words.csv">top_words.csv</a>
  <a class="button" href="top_artists.csv">top_artists.csv</a>
  <a class="button" href="wordcount.csv">wordcount.csv</a>
  <a class="button" href="artists.csv">artists.csv</a>
</p>

<div class="grid">
  <div class="card">
    <h3>Wordcount — Tempo por P</h3>
    <canvas id="wc_time"></canvas>
  </div>
  <div class="card">
    <h3>Wordcount — Speedup & Eficiência</h3>
    <canvas id="wc_speed_eff"></canvas>
  </div>
  <div class="card">
    <h3>Artistcount — Tempo por P</h3>
    <canvas id="ac_time"></canvas>
  </div>
  <div class="card">
    <h3>Artistcount — Speedup & Eficiência</h3>
    <canvas id="ac_speed_eff"></canvas>
  </div>
  <div class="card">
    <h3>Sentiment HF — Tempo vs Batch</h3>
    <canvas id="se_time"></canvas>
  </div>
</div>

<script>
const wc_x = {json.dumps(wc_x)};
const wc_t = {json.dumps(wc_t)};
const wc_s = {json.dumps(wc_s)};
const wc_e = {json.dumps(wc_e)};

const ac_x = {json.dumps(ac_x)};
const ac_t = {json.dumps(ac_t)};
const ac_s = {json.dumps(ac_s)};
const ac_e = {json.dumps(ac_e)};

const se_b = {json.dumps(se_b)};
const se_t = {json.dumps(se_t)};

function lineChart(id, labels, data, label) {{
  new Chart(document.getElementById(id), {{
    type: 'line',
    data: {{ labels, datasets: [{{ label, data, tension: .2 }}] }},
    options: {{ responsive: true, interaction: {{ mode: 'index', intersect: false }}, plugins: {{ legend: {{ display: true }} }} }}
  }});
}}

function multiAxisChart(id, labels, sData, eData) {{
  new Chart(document.getElementById(id), {{
    type: 'line',
    data: {{
      labels,
      datasets: [
        {{ label: 'Speedup', data: sData, yAxisID: 'y1', tension: .2 }},
        {{ label: 'Eficiência', data: eData, yAxisID: 'y2', tension: .2 }}
      ]
    }},
    options: {{
      responsive: true,
      interaction: {{ mode: 'index', intersect: false }},
      scales: {{
        y1: {{ type: 'linear', position: 'left' }},
        y2: {{ type: 'linear', position: 'right', min: 0, max: 1 }}
      }}
    }}
  }});
}}

lineChart('wc_time', wc_x, wc_t, 'Tempo (s)');
multiAxisChart('wc_speed_eff', wc_x, wc_s, wc_e);
lineChart('ac_time', ac_x, ac_t, 'Tempo (s)');
multiAxisChart('ac_speed_eff', ac_x, ac_s, ac_e);
lineChart('se_time', se_b, se_t, 'Tempo (s)');
</script>
</body></html>
"""

def main():
    wc_x, wc_t = load_csv_times(WC)
    ac_x, ac_t = load_csv_times(AC)
    se_b, _, se_t = load_sentiment(SE)

    wc_s, wc_e, _ = compute_speedup_eff(wc_x, wc_t)
    ac_s, ac_e, _ = compute_speedup_eff(ac_x, ac_t)

    html = html_page(wc_x, wc_t, wc_s, wc_e, ac_x, ac_t, ac_s, ac_e, se_b, se_t)
    DASH.write_text(html, encoding="utf-8")
    INDEX.write_text('<!doctype html><html><head><meta charset="utf-8"><title>Index</title></head>'
                     '<body><h3>Arquivos</h3><ul>'
                     '<li><a href="dashboard.html">Dashboard</a></li>'
                     '<li><a href="tempos_wordcount.csv">tempos_wordcount.csv</a></li>'
                     '<li><a href="tempos_artistcount.csv">tempos_artistcount.csv</a></li>'
                     '<li><a href="tempos_sentiment.csv">tempos_sentiment.csv</a></li>'
                     '<li><a href="top_words.csv">top_words.csv</a></li>'
                     '<li><a href="top_artists.csv">top_artists.csv</a></li>'
                     '<li><a href="wordcount.csv">wordcount.csv</a></li>'
                     '<li><a href="artists.csv">artists.csv</a></li>'
                     '</ul></body></html>', encoding="utf-8")
    print(f"[OK] Gerado: {DASH}")
    print(f"[OK] Gerado: {INDEX}")

if __name__ == "__main__":
    main()
