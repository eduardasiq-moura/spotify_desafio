#ifndef COMMON_HASH_H
#define COMMON_HASH_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*
 * Estruturas básicas da nossa hash table de contagem:
 * - HashNode: nó encadeado (chave string + contador + próximo)
 * - HashMap: vetor de buckets com encadeamento por colisão
 */
typedef struct HashNode {
    char *key;           // chave (string alocada dinamicamente)
    long count;          // contador acumulado
    struct HashNode *next; // próximo nó no bucket (encadeamento)
} HashNode;

typedef struct {
    HashNode **buckets;  // vetor de listas (tabela hash)
    size_t nbuckets;     // quantidade de buckets
    size_t nitems;       // quantidade total de itens (nós)
} HashMap;

/*
 * Função hash djb2: simples, boa distribuição para strings curtas.
 * Mantemos unsigned long e mod nbuckets ao indexar.
 */
static unsigned long hash_str(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + c; // h*33 + c
    return h;
}

/* Cria uma hash nova com nbuckets (ou 1024 por padrão) */
static HashMap* h_new(size_t nbuckets) {
    HashMap *m = (HashMap*)calloc(1, sizeof(HashMap));
    m->nbuckets = nbuckets ? nbuckets : 1024;
    m->buckets = (HashNode**)calloc(m->nbuckets, sizeof(HashNode*));
    return m;
}

/* Libera toda a memória (nós, chaves e vetor de buckets) */
static void h_free(HashMap *m) {
    if (!m) return;
    for (size_t i = 0; i < m->nbuckets; ++i) {
        HashNode *cur = m->buckets[i];
        while (cur) {
            HashNode *nx = cur->next;
            free(cur->key);  // chave foi alocada (malloc/strdup-like)
            free(cur);
            cur = nx;
        }
    }
    free(m->buckets);
    free(m);
}

/*
 * h_add: soma 'delta' à chave.
 * - Se a chave existir no bucket, apenas incrementa.
 * - Se não existir, cria novo nó no início da lista.
 * Observações:
 * - Evitamos inserir chaves vazias.
 * - Duplicamos a string 'key' manualmente (sem depender de strdup POSIX).
 */
static void h_add(HashMap *m, const char *key, long delta) {
    if (!key || !*key) return;
    unsigned long h = hash_str(key) % m->nbuckets;
    HashNode *cur = m->buckets[h];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            cur->count += delta; // chave já existe → acumula
            return;
        }
        cur = cur->next;
    }
    // novo nó (chave ainda não existente)
    HashNode *n = (HashNode*)calloc(1, sizeof(HashNode));
    // duplicação manual da string (compatível com C11)
    size_t len = strlen(key) + 1;
    n->key = (char*)malloc(len);
    if (n->key) memcpy(n->key, key, len);
    n->count = delta;
    n->next = m->buckets[h];  // insere na cabeça da lista
    m->buckets[h] = n;
    m->nitems++;
}

/*
 * Iterador genérico: percorre todos os buckets e nós chamando 'fn'.
 * Útil para serialização e merge final no rank 0 (MPI).
 */
typedef void (*h_iter_fn)(const char *key, long count, void *ud);

static void h_foreach(HashMap *m, h_iter_fn fn, void *ud) {
    for (size_t i = 0; i < m->nbuckets; ++i) {
        HashNode *cur = m->buckets[i];
        while (cur) {
            fn(cur->key, cur->count, ud);
            cur = cur->next;
        }
    }
}

#endif
