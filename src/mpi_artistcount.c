// mpi_artistcount.c
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_hash.h"

#define LINE_MAX 131072

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank=0, size=1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const char *infile  = "data/songs.tsv";
    const char *outfile = "out/artists.csv";

    double t0 = MPI_Wtime();

    FILE *f = fopen(infile, "r");
    if (!f) {
        if (rank==0) fprintf(stderr, "Erro abrindo %s\n", infile);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    HashMap *local = h_new(1<<14);

    char line[LINE_MAX];
    long long lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        if ((lineno % size) == rank) {
            char *tab = strchr(line, '\t');
            if (!tab) { lineno++; continue; }
            // isola artist
            *tab = '\0';
            char *artist = line;
            // remove newline eventual ao final do texto (não necessário aqui)
            // conta 1 música para o artista
            if (*artist) h_add(local, artist, 1);
        }
        lineno++;
    }
    fclose(f);

    // Serializa local (artist\tcount\n)
    typedef struct { char *buf; size_t len; size_t cap; } SBuf;
    void sb_init(SBuf *s){ s->buf=NULL; s->len=0; s->cap=0; }
    void sb_append(SBuf *s, const char *str){
        size_t need = strlen(str);
        if (s->len + need + 1 > s->cap) {
            s->cap = (s->len + need + 65536);
            s->buf = (char*)realloc(s->buf, s->cap);
        }
        memcpy(s->buf + s->len, str, need);
        s->len += need;
        s->buf[s->len] = '\0';
    }
    void emit(const char *k, long c, void *ud){
        SBuf *s = (SBuf*)ud;
        char line[4096];
        snprintf(line, sizeof(line), "%s\t%ld\n", k, c);
        sb_append(s, line);
    }

    SBuf sb; sb_init(&sb);
    h_foreach(local, emit, &sb);
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
        HashMap *global = h_new(1<<15);
        int offset = 0;
        for (int r=0; r<size; ++r) {
            int L = lens[r], end = offset + L;
            while (offset < end) {
                char *lineptr = recvbuf + offset;
                char *nl = memchr(lineptr, '\n', end - offset);
                int linelen = (int)(nl ? (nl - lineptr) : (end - offset));
                if (linelen <= 0) { offset = nl? (nl+1 - recvbuf) : end; continue; }
                char *tab = memchr(lineptr, '\t', linelen);
                if (tab) {
                    int klen = (int)(tab - lineptr);
                    int vlen = linelen - klen - 1;
                    char keybuf[4096], valbuf[64];
                    int kl = (klen < (int)sizeof(keybuf)-1) ? klen : (int)sizeof(keybuf)-1;
                    int vl = (vlen < (int)sizeof(valbuf)-1) ? vlen : (int)sizeof(valbuf)-1;
                    memcpy(keybuf, lineptr, kl); keybuf[kl]='\0';
                    memcpy(valbuf, tab+1, vl);   valbuf[vl]='\0';
                    long cnt = strtol(valbuf, NULL, 10);
                    h_add(global, keybuf, cnt);
                }
                offset += (linelen + (nl?1:0));
            }
        }

        FILE *fo = fopen(outfile, "w");
        if (!fo) {
            fprintf(stderr, "Erro abrindo %s\n", outfile);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
        fprintf(fo, "artist;num_songs\n");
        void writer(const char *k, long c, void *ud) {
            FILE *fp = (FILE*)ud;
            fprintf(fp, "%s;%ld\n", k, c);
        }
        h_foreach(global, writer, fo);
        fclose(fo);
        h_free(global);

        fprintf(stdout, "[artistcount] linhas=%lld, procs=%d, t_total=%.3fs\n",
                lineno, size, (t1 - t0));
    }

    free(lens); free(displs); free(recvbuf);
    free(sb.buf);
    h_free(local);
    MPI_Finalize();
    return 0;
}
