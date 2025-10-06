#!/usr/bin/env python3
import csv, json
from pathlib import Path

OUT = Path("out")
OUT.mkdir(exist_ok=True)

WC = OUT/"tempos_wordcount.csv"
AC = OUT/"tempos_artistcount.csv"
SE = OUT/"tempos_sentiment.csv"
DASH = OUT/"dashboard.html"

def load_csv_times(path):
    xs, ys = [], []
    if not path.exists():
        return xs, ys
    with open(path, encoding="utf-8") as f:
        rdr = csv.reader(f)
        next(rdr, None)
        for row in rdr:
            if not row: continue
            xs.append(int(row[0]))
            ys.append(float(row[1]) if row[1] else 0)
    return xs, ys

def compute_speedup_eff(x, t):
    if not t: return [], [], 0
    T1 = t[0] if t[0] != 0 else 1
    s = [T1/v if v else 0 for v in t]
    e = [sp/i if i else 0 for sp,i in zip(s,x)]
    return s,e,T1

def load_sentiment(path):
    b,m,t = [],[],[]
    if not path.exists(): return b,m,t
    with open(path,encoding="utf-8") as f:
        rdr = csv.reader(f); next(rdr,None)
        for row in rdr:
            if not row: continue
            b.append(int(row[0])); m.append(int(row[1])); t.append(float(row[2]) if row[2] else 0)
    return b,m,t

def main():
    wc_x,wc_t = load_csv_times(WC)
    ac_x,ac_t = load_csv_times(AC)
    se_b,_,se_t = load_sentiment(SE)

    wc_s,wc_e,_ = compute_speedup_eff(wc_x,wc_t)
    ac_s,ac_e,_ = compute_speedup_eff(ac_x,ac_t)

    html=f"""<!doctype html><html lang='pt-br'>
<head>
<meta charset='utf-8'>
<title>Dashboard de Métricas - Spotify MPI + HF</title>
<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js'></script>
<style>
body{{font-family:system-ui,Arial;margin:20px;background:#fafafa}}
h1{{margin-bottom:4px}}
.grid{{display:grid;gap:20px;grid-template-columns:repeat(auto-fit,minmax(380px,1fr))}}
.card{{background:#fff;border:1px solid #ddd;border-radius:10px;padding:16px;box-shadow:0 2px 5px rgba(0,0,0,.05)}}
canvas{{width:100%!important;height:300px!important}}
</style>
</head><body>
<h1>Dashboard de Métricas</h1>
<small>Base gerada automaticamente (arquivos em out/)</small>
<div class='grid'>
  <div class='card'><h3>Wordcount — Tempo</h3><canvas id='wc_time'></canvas></div>
  <div class='card'><h3>Wordcount — Speedup</h3><canvas id='wc_speed'></canvas></div>
  <div class='card'><h3>Artistcount — Tempo</h3><canvas id='ac_time'></canvas></div>
  <div class='card'><h3>Artistcount — Speedup</h3><canvas id='ac_speed'></canvas></div>
  <div class='card'><h3>Sentiment — Tempo vs Batch</h3><canvas id='sent'></canvas></div>
</div>
<script>
const wc_x={json.dumps(wc_x)},wc_t={json.dumps(wc_t)},wc_s={json.dumps(wc_s)};
const ac_x={json.dumps(ac_x)},ac_t={json.dumps(ac_t)},ac_s={json.dumps(ac_s)};
const se_b={json.dumps(se_b)},se_t={json.dumps(se_t)};
function mk(id,lbl,x,y){{
  new Chart(document.getElementById(id),{{type:'line',data:{{labels:x,datasets:[{{label:lbl,data:y,tension:.2,borderWidth:2}}]}}}});
}}
mk('wc_time','Tempo (s)',wc_x,wc_t);
mk('wc_speed','Speedup',wc_x,wc_s);
mk('ac_time','Tempo (s)',ac_x,ac_t);
mk('ac_speed','Speedup',ac_x,ac_s);
mk('sent','Tempo (s)',se_b,se_t);
</script></body></html>"""
    DASH.write_text(html,encoding="utf-8")
    print(f"[OK] Dashboard gerado em {DASH}")

if __name__=="__main__":
    main()
