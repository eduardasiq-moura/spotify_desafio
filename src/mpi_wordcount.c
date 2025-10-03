// mpi_wordcount.c
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common_hash.h"

#define LINE_MAX 131072  // buffer por linha (128KB)
#define BUF_CHUNK 65536  // para serialização

// Pequena lista de stopwords (você pode ampliar/alterar)
static const char* STOPWORDS[] = {
    "a","o","os","as","de","da","do","das","dos","e","ou","um","uma","uns","umas",
    "the","and","or","to","of","in","on","for","is","it","that","this","i","you",
    NULL
};

static int is_stopword(const char *w) {
    for (int i = 0; STOPWORDS[i]; ++i)
        if (strcmp(w, STOPWORDS[i]) == 0) return 1;
    return 0;
}

static void to_lower_ascii(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

// Tokenizador simples: separa por não [a-z0-9]
static void count_words(HashMap *map, const char *text) {
    size_t n = strlen(text);
    char *tmp = (char*)malloc(n+1);
    strcpy(tmp, text);
    to_lower_ascii(tmp);

    char *p = tmp, *start = NULL;
    while (1) {
        char c = *p;
        int is_alnum = (c && ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')));
        if (is_alnum) {
            if (!start) start = p;
        } else {
            if (start) {
                char saved = *p;
                *p = '\0';
                if (!is_stopword(start)) {
                    h_add(map, start, 1);
                }
                *p = saved;
                start = NULL;
            }
            if (c == '\0') break;
        }
        p++;
    }
    free(tmp);
}

// Serializa HashMap em "key\tcount\n" para enviar ao rank 0
typedef struct { char *buf; size_t len; size_t cap; } SBuf;

static void sb_init(SBuf *s) { s->buf=NULL; s->len=0; s->cap=0; }
static void sb_append(SBuf *s, const char *str) {
    size_t need = strlen(str);
    if (s->len + need + 1 > s->cap) {
        s->cap = (s->len + need + BUF_CHUNK);
        s->buf = (char*)realloc(s->buf, s->cap);
    }
    memcpy(s->buf + s->len, str, need);
    s->len += need;
    s->buf[s->len] = '\0';
}
static void emit_kv(const char *key, long count, void *ud) {
    SBuf *s = (SBuf*)ud;
    char line[4096];
    snprintf(line, sizeof(line), "%s\t%ld\n", key, count);
    sb_append(s, line);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank=0, size=1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const char *infile = "data/songs.tsv";
    const char *outfile = "out/wordcount.csv";

    double t0 = MPI_Wtime();

    FILE *f = fopen(infile, "r");
    if (!f) {
        if (rank==0) fprintf(stderr, "Erro abrindo %s\n", infile);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    HashMap *local = h_new(1<<15); // 32768 buckets

    char line[LINE_MAX];
    long long lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        // round-robin por linha
        if ((lineno % size) == rank) {
            // Formato: artist \t text
            char *tab = strchr(line, '\t');
            if (!tab) { lineno++; continue; }
            char *text = tab + 1;
            // remove \n final
            size_t L = strlen(text);
            if (L && (text[L-1] == '\n' || text[L-1]=='\r')) text[L-1]='\0';
            count_words(local, text);
        }
        lineno++;
    }
    fclose(f);

    // Serializa parciais
    SBuf sb; sb_init(&sb);
    h_foreach(local, emit_kv, &sb);

    // Comunicação: todos enviam seu buffer para o rank 0
    int mylen = (int)sb.len;
    int *lens = NULL;
    if (rank==0) lens = (int*)calloc(size, sizeof(int));
    MPI_Gather(&mylen, 1, MPI_INT, lens, 1, MPI_INT, 0, MPI_COMM_WORLD);

    char *recvbuf = NULL;
    int *displs = NULL;
    int totallen = 0;
    if (rank==0) {
        displs = (int*)calloc(size, sizeof(int));
        for (int i=0;i<size;i++){ displs[i]=totallen; totallen+=lens[i]; }
        recvbuf = (char*)malloc(totallen+1);
    }

    MPI_Gatherv(sb.buf, mylen, MPI_CHAR, recvbuf, lens, displs, MPI_CHAR, 0, MPI_COMM_WORLD);

    double t1 = MPI_Wtime();

    if (rank==0) {
        // Merge no root
        HashMap *global = h_new(1<<16);
        int offset = 0;
        for (int r=0; r<size; ++r) {
            int L = lens[r];
            int end = offset + L;
            while (offset < end) {
                // linha "key \t count \n"
                char *lineptr = recvbuf + offset;
                // encontra '\n'
                char *nl = memchr(lineptr, '\n', end - offset);
                int linelen = (int)(nl ? (nl - lineptr) : (end - offset));
                if (linelen <= 0) { offset = nl? (nl+1 - recvbuf) : end; continue; }

                // separa tab
                char *tab = memchr(lineptr, '\t', linelen);
                if (tab) {
                    int klen = (int)(tab - lineptr);
                    int vlen = linelen - klen - 1;
                    char keybuf[4096];
                    char valbuf[64];
                    int kl = (klen < (int)sizeof(keybuf)-1) ? klen : (int)sizeof(keybuf)-1;
                    int vl = (vlen < (int)sizeof(valbuf)-1) ? vlen : (int)sizeof(valbuf)-1;
                    memcpy(keybuf, lineptr, kl); keybuf[kl]='\0';
                    memcpy(valbuf, tab+1, vl); valbuf[vl]='\0';
                    long cnt = strtol(valbuf, NULL, 10);
                    h_add(global, keybuf, cnt);
                }
                offset += (linelen + (nl?1:0));
            }
        }

        // Salva CSV: palavra;count
        FILE *fo = fopen(outfile, "w");
        if (!fo) {
            fprintf(stderr, "Erro abrindo %s para escrita\n", outfile);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
        fprintf(fo, "word;count\n");
        void writer(const char *k, long c, void *ud) {
            FILE *fp = (FILE*)ud;
            fprintf(fp, "%s;%ld\n", k, c);
        }
        h_foreach(global, writer, fo);
        fclose(fo);
        h_free(global);

        fprintf(stdout, "[wordcount] linhas=%lld, procs=%d, t_total=%.3fs\n",
                lineno, size, (t1 - t0));
    }

    free(lens); free(displs); free(recvbuf);
    free(sb.buf);
    h_free(local);

    MPI_Finalize();
    return 0;
}
