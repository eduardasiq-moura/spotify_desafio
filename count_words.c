// mpi_wc_simplificado.c
// Word count simples com MPI: rank 0 lê CSV e distribui letras; workers contam e devolvem.
// Foco didático: parsing simplificado e dicionário via lista ligada.

#include <mpi.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Lista ligada (palavra -> contagem) ---------- */
typedef struct Node {
    char *word;
    unsigned long long count;
    struct Node *next;
} Node;

static Node* add_or_inc(Node *head, const char *w) {
    for (Node *p = head; p; p = p->next) {
        if (strcmp(p->word, w) == 0) { p->count++; return head; }
    }
    Node *n = (Node*)malloc(sizeof(Node));
    n->word = strdup(w);
    n->count = 1;
    n->next = head;
    return n;
}

static void free_list(Node *head) {
    while (head) { Node *nx = head->next; free(head->word); free(head); head = nx; }
}

/* ---------- Tokenização (ASCII) ---------- */
static void tokenize_and_count(const char *text, Node **dict) {
    char buf[256];
    int n = 0;
    for (const unsigned char *p = (const unsigned char*)text; *p; ++p) {
        if (isalpha(*p)) {
            if (n < 255) buf[n++] = (char)tolower(*p);
        } else {
            if (n > 0) { buf[n] = '\0'; *dict = add_or_inc(*dict, buf); n = 0; }
        }
    }
    if (n > 0) { buf[n] = '\0'; *dict = add_or_inc(*dict, buf); }
}

/* ---------- Serialização simples (texto) ---------- */
/* Converte a lista em um único buffer de linhas "palavra contagem\n" */
static char* dict_to_text(Node *head, int *out_len) {
    size_t cap = 1<<16, len = 0;
    char *buf = (char*)malloc(cap);
    if (!buf) { perror("malloc"); exit(1); }

    for (Node *p = head; p; p = p->next) {
        char line[512];
        int m = snprintf(line, sizeof(line), "%s %llu\n", p->word, p->count);
        if (m < 0) continue;
        if (len + (size_t)m + 1 > cap) {
            while (len + (size_t)m + 1 > cap) cap *= 2;
            char *tmp = (char*)realloc(buf, cap);
            if (!tmp) { perror("realloc"); exit(1); }
            buf = tmp;
        }
        memcpy(buf + len, line, (size_t)m);
        len += (size_t)m;
    }
    buf[len] = '\0';
    *out_len = (int)len;
    return buf;
}

/* ---------- Agregação no mestre a partir de texto ---------- */
static Node* merge_text_into_dict(const char *buf, int len, Node *dict) {
    // cada linha: "palavra contagem"
    const char *p = buf, *end = buf + len;
    while (p < end) {
        // acha fim de linha
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t L = nl ? (size_t)(nl - p) : (size_t)(end - p);
        if (L > 0) {
            // separa último espaço
            const char *sp = p + L - 1;
            while (sp > p && *sp != ' ') sp--;
            if (sp > p && *sp == ' ') {
                size_t wlen = (size_t)(sp - p);
                char *w = (char*)malloc(wlen + 1);
                memcpy(w, p, wlen); w[wlen] = '\0';
                unsigned long long c = strtoull(sp + 1, NULL, 10);
                // soma c
                for (Node *q = dict; ; q = q ? q->next : NULL) {
                    if (!q) { dict = add_or_inc(dict, w); dict->count += (c - 1); break; }
                    if (strcmp(q->word, w) == 0) { q->count += c; break; }
                }
                free(w);
            }
        }
        p = nl ? (nl + 1) : end;
    }
    return dict;
}

/* ---------- Leitura do CSV (bem simplificado) ----------
   Regras assumidas:
   - Linhas são: artist,song,link,"TEXTO...."
   - As 3 primeiras colunas NÃO têm vírgula.
   - O campo de texto começa na primeira aspa após a 3ª vírgula.
   - Pode ter quebras de linha dentro do texto.
   - NÃO tratamos aspas escapadas "" (se existirem muito, melhorar depois).
*/
static char* read_lyric_block(FILE *fp) {
    static char *acc = NULL;      // acumulador para a letra
    static size_t cap = 0, len = 0;

    free(acc); acc = NULL; cap = len = 0;

    char line[1<<14]; // 16 KiB por linha (OK para maioria)
    if (!fgets(line, sizeof(line), fp)) return NULL; // EOF

    // conta 3 vírgulas
    int commas = 0; size_t i = 0;
    while (line[i] && commas < 3) { if (line[i] == ',') commas++; i++; }
    if (commas < 3) return NULL; // linha inesperada

    // procura aspa inicial do campo texto
    while (line[i] && line[i] != '"') i++;
    if (line[i] != '"') return NULL;
    i++; // pula a aspa

    // inicia acumulador
    cap = 1<<16; acc = (char*)malloc(cap); if(!acc){perror("malloc"); exit(1);}
    len = 0;

    // copia o resto da linha após a aspa inicial
    for (; line[i]; ++i) {
        if (line[i] == '"') {
            // se fecha aqui na mesma linha? assumimos que sim se aspas aparece antes do fim (sem escape)
            // mas texto pode continuar; verificamos se depois vem vírgula ou fim de linha
            // para simplificar: se achou aspa, consideramos fim do texto
            goto DONE_BLOCK;
        }
        if (len + 2 > cap) { cap *= 2; acc = (char*)realloc(acc, cap); if(!acc){perror("realloc"); exit(1);} }
        acc[len++] = line[i];
    }
    // não fechou; então continuamos lendo linhas até achar uma aspa
    while (fgets(line, sizeof(line), fp)) {
        for (size_t j = 0; line[j]; ++j) {
            if (line[j] == '"') { goto DONE_BLOCK; }
            if (len + 2 > cap) { cap *= 2; acc = (char*)realloc(acc, cap); if(!acc){perror("realloc"); exit(1);} }
            acc[len++] = line[j];
        }
        // se não tinha aspa nesta linha, preserva \n (fgets mantém)
        if (len + 2 > cap) { cap *= 2; acc = (char*)realloc(acc, cap); if(!acc){perror("realloc"); exit(1);} }
        // (já copiamos o \n acima porque fgets inclui; então nada a fazer extra)
    }

DONE_BLOCK:
    if (!acc) return NULL;
    acc[len] = '\0';
    return acc;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size; MPI_Comm_rank(MPI_COMM_WORLD, &rank); MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0) fprintf(stderr, "Use ao menos 2 processos: mpirun -n 4 ./prog arquivo.csv\n");
        MPI_Finalize(); return 1;
    }

    if (rank == 0) {
        if (argc < 2) { fprintf(stderr, "Uso: mpirun -n <P> %s arquivo.csv\n", argv[0]); MPI_Finalize(); return 1; }
        FILE *fp = fopen(argv[1], "rb");
        if (!fp) { perror("abrindo CSV"); MPI_Abort(MPI_COMM_WORLD, 2); }

        int next = 1;
        // Lê letras e envia para workers (tamanho + bytes)
        while (1) {
            char *lyric = read_lyric_block(fp);
            if (!lyric) break;
            int L = (int)strlen(lyric);
            int dst = next; if (dst >= size) dst = 1;
            MPI_Send(&L, 1, MPI_INT, dst, 1, MPI_COMM_WORLD);
            MPI_Send(lyric, L, MPI_CHAR, dst, 1, MPI_COMM_WORLD);
            next = dst + 1;
            free(lyric);
        }
        fclose(fp);
        // Sinal de término
        int end = -1;
        for (int w = 1; w < size; ++w) MPI_Send(&end, 1, MPI_INT, w, 1, MPI_COMM_WORLD);

        // Recebe resultados
        Node *global = NULL;
        for (int w = 1; w < size; ++w) {
            int nbytes; MPI_Recv(&nbytes, 1, MPI_INT, w, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (nbytes <= 0) continue;
            char *buf = (char*)malloc((size_t)nbytes);
            MPI_Recv(buf, nbytes, MPI_CHAR, w, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            global = merge_text_into_dict(buf, nbytes, global);
            free(buf);
        }

        // Imprime (uma linha por palavra). Quer ordenar? redirecione e use `sort`.
        for (Node *p = global; p; p = p->next) {
            printf("%s %llu\n", p->word, p->count);
        }
        free_list(global);
    } else {
        Node *local = NULL;
        while (1) {
            int L; MPI_Recv(&L, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (L == -1) break;
            char *buf = (char*)malloc((size_t)L + 1);
            MPI_Recv(buf, L, MPI_CHAR, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            buf[L] = '\0';
            tokenize_and_count(buf, &local);
            free(buf);
        }
        int nbytes = 0; char *payload = dict_to_text(local, &nbytes);
        MPI_Send(&nbytes, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
        if (nbytes > 0) MPI_Send(payload, nbytes, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
        free(payload);
        free_list(local);
    }

    MPI_Finalize();
    return 0;
}
