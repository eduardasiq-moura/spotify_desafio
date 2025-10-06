# Spotify – MPI + Sentimento (HF)

Desenvolvido por: Gustavo Straliotto Drews, Eduarda Siqueira de Moura e Laisa Pletsch

Aplicação acadêmica para processar o dataset **Spotify Million Song Dataset** em **paralelo (MPI/C)** e classificar sentimento das letras com **Hugging Face (Python)**.

## 📦 O que este projeto faz

1. **Pré-processa** o CSV original do Kaggle e gera um TSV seguro (`artist \t text`).
2. **MPI – Contagem de Palavras**: conta a frequência de cada palavra nas letras (C + MPI).
3. **MPI – Artistas com mais músicas**: conta quantas músicas por artista (C + MPI).
4. **Classificação de Sentimento**: rotula letras como **Positiva / Neutra / Negativa** (Python + Transformers/HF).

---

## ✅ Pré-requisitos (WSL Ubuntu)

Instale os pacotes base (C, MPI, Python, Make):

```bash
sudo apt update
sudo apt install -y mpich python3 python3-venv python3-pip build-essential make
```

> **Observação**: o projeto foi pensado para **WSL**.

---

## 🐍 Ambiente Python (venv) + dependências

Na raiz do projeto:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

> Caso rode classificação com Transformers/HF e ainda não tenha as libs correspondentes no seu `requirements.txt`, garanta algo como:
> ```
> transformers>=4.44
> torch>=2.3
> tqdm
> sentencepiece
> protobuf
> ```

---

## 🎵 Colocando o dataset

Coloque `spotify_millsongdata.csv` dentro de `data/`.

---

## 🛠️ 1) Pré-processar o CSV → TSV

Gera `data/songs.tsv` (uma linha por música: `artist \t text`).

```bash
source .venv/bin/activate
python3 src/preprocess_csv.py
```

Saída esperada:

```
OK: XXXXXXX registros → data/songs.tsv
```

---

## ⚙️ 2) Compilar (MPI/C)

O `Makefile` já está configurado. Na raiz do projeto:

```bash
make
```

Isso cria os binários em `build/`:
- `build/mpi_wordcount`
- `build/mpi_artistcount`

Se precisar limpar:

```bash
make clean && make
```

---

## 🚀 3) Executar (MPI)

> Ajuste `-np` conforme seu número de núcleos (ex.: 2, 4, 8).

### 3.1 Contagem de palavras
```bash
mpirun -np 4 ./build/mpi_wordcount
```
- **Saída**: `out/wordcount.csv` (`word;count`)

### 3.2 Artistas com mais músicas
```bash
mpirun -np 4 ./build/mpi_artistcount
```
- **Saída**: `out/artists.csv` (`artist;num_songs`)

**Top-N no terminal:**
```bash
# Top 20 palavras
LC_ALL=C sort -t';' -k2,2nr out/wordcount.csv | head -n 21 | column -s';' -t

# Top 20 artistas
LC_ALL=C sort -t';' -k2,2nr out/artists.csv   | head -n 21 | column -s';' -t
```

---

## 🧠 4) Classificação de sentimento (Hugging Face)

O script padrão usa um modelo **leve** multilíngue (recomendado para CPU):

```bash
source .venv/bin/activate
python3 src/sentiment_hf.py   --model lxyuan/distilbert-base-multilingual-cased-sentiments-student   --max 200 --batch 8 --device cpu   --out out/sentiment_counts.csv
```

- `--max`: limite de músicas para testes rápidos (remova para processar tudo).
- `--batch`: tente 8–16 na CPU (ajuste se faltar RAM).
- `--device cuda`: se você tiver GPU CUDA habilitada no WSL.

**Resultados**: `out/sentiment_counts.csv` (3 classes: `Positiva`, `Neutra`, `Negativa`).

---

# 📊 5) Análise de Métricas de Desempenho (MPI)

As métricas de desempenho foram calculadas com base no **Tempo Total de Execução** ($T_p$) em função do número de processos ($P$), comparando o tempo de execução paralela com o tempo sequencial ($T_1$).

### 🧮 Fórmulas Utilizadas:

- **Speedup** ($S_p$):  
  $$
  S_p = \frac{T_1}{T_p}
  $$

- **Eficiência** ($E_p$):  
  $$
  E_p = \frac{S_p}{P}
  $$

### 1. Desempenho da Contagem de Palavras (`mpi_wordcount`)

| **P (Processos)** | **$T_p$ (Tempo Total)** | **Speedup ($S_p$)** | **Eficiência ($E_p$)** | **Ponto Ótimo** |
|:-----------------:|:----------------------:|:-------------------:|:----------------------:|:---------------:|
| 1  | 107.444s | 1.00x | 1.00 | - |
| 2  | 33.184s  | 3.24x | 1.62 | - |
| **4**  | **26.520s** | **4.05x** | **1.01** | ✅ |
| 6  | 32.416s  | 3.31x | 0.55 | - |
| 12 | 54.704s  | 1.96x | 0.16 | - |

**Conclusão:** A tarefa de contagem de palavras atinge seu melhor desempenho com 4 processos, alcançando um Speedup de 4.05x. A eficiência de 1.01 (próxima do ideal 1.00) indica que, nessa configuração, o custo de comunicação é minimizado. A partir de P=6, o overhead do MPI e a sobrecarga de comunicação começam a prejudicar os ganhos.

### 2. Desempenho da Contagem de Artistas (`mpi_artistcount`)

| **P (Processos)** | **$T_p$ (Tempo Total)** | **Speedup ($S_p$)** | **Eficiência ($E_p$)** | **Ponto Ótimo** |
|:-----------------:|:----------------------:|:-------------------:|:----------------------:|:---------------:|
| 1  | 108.739s | 1.00x | 1.00 | - |
| **2**  | **19.541s** | **5.56x** | **2.78** | ✅ |
| 4  | 33.839s  | 3.21x | 0.80 | - |
| 6  | 35.988s  | 3.02x | 0.50 | - |
| 12 | 56.166s  | 1.94x | 0.16 | - |

**Conclusão:** A tarefa de contagem dos artistas, mostra uma forte escalabilidade inicial, atingindo o **pico de desempenho** com apenas **2 processos** (Speedup de **5.56x**). O valor de Eficiência ($E_p = 2.78$) é **superlinear** ($E_p > 1$), o que geralmente indica que o aumento da memória total de cache disponível para os processos resultou em uma redução significativa no tempo de acesso a dados, beneficiando o desempenho muito além do esperado.   No entanto, o ganho desaparece rapidamente em $P = 4$ e configurações maiores.


```bash
# exemplo com wordcount
mpirun --oversubscribe -np 1  ./build/mpi_wordcount
mpirun --oversubscribe -np 2  ./build/mpi_wordcount
mpirun --oversubscribe -np 4  ./build/mpi_wordcount
mpirun --oversubscribe -np 6  ./build/mpi_wordcount
mpirun --oversubscribe -np 12 ./build/mpi_wordcount

# exemplo com artistcount
mpirun --oversubscribe -np 1  ./build/mpi_artistcount
mpirun --oversubscribe -np 2  ./build/mpi_artistcount
mpirun --oversubscribe -np 4  ./build/mpi_artistcount
mpirun --oversubscribe -np 6  ./build/mpi_artistcount
mpirun --oversubscribe -np 12 ./build/mpi_artistcount

```


---

## 🔁 Comandos essenciais (cola e roda)

```bash
# 0) venv + deps
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# 1) preparar dataset
python3 src/preprocess_csv.py   # data/songs.tsv

# 2) compilar MPI
make

# 3) executar MPI
mpirun -np 4 ./build/mpi_wordcount    # out/wordcount.csv
mpirun -np 4 ./build/mpi_artistcount  # out/artists.csv

# 4) sentimento (modelo leve)
python3 src/sentiment_hf.py   --model lxyuan/distilbert-base-multilingual-cased-sentiments-student   --max 200 --batch 8 --device cpu   --out out/sentiment_counts.csv
```

