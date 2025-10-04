// common_hash.h
#ifndef COMMON_HASH_H
#define COMMON_HASH_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct HashNode {
    char *key;
    long count;
    struct HashNode *next;
} HashNode;

typedef struct {
    HashNode **buckets;
    size_t nbuckets;
    size_t nitems;
} HashMap;

// djb2 hash (boa, simples)
static unsigned long hash_str(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + c;
    return h;
}

static HashMap* h_new(size_t nbuckets) {
    HashMap *m = (HashMap*)calloc(1, sizeof(HashMap));
    m->nbuckets = nbuckets ? nbuckets : 1024;
    m->buckets = (HashNode**)calloc(m->nbuckets, sizeof(HashNode*));
    return m;
}

static void h_free(HashMap *m) {
    if (!m) return;
    for (size_t i = 0; i < m->nbuckets; ++i) {
        HashNode *cur = m->buckets[i];
        while (cur) {
            HashNode *nx = cur->next;
            free(cur->key);
            free(cur);
            cur = nx;
        }
    }
    free(m->buckets);
    free(m);
}

// Adiciona +delta à chave; cria se não existe
static void h_add(HashMap *m, const char *key, long delta) {
    if (!key || !*key) return;
    unsigned long h = hash_str(key) % m->nbuckets;
    HashNode *cur = m->buckets[h];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            cur->count += delta;
            return;
        }
        cur = cur->next;
    }
    // novo
    HashNode *n = (HashNode*)calloc(1, sizeof(HashNode));
    // duplicação de string sem usar strdup (POSIX)
    size_t len = strlen(key) + 1;
    n->key = (char*)malloc(len);
    if (n->key) memcpy(n->key, key, len);
    n->count = delta;
    n->next = m->buckets[h];
    m->buckets[h] = n;
    m->nitems++;
}


// Itera (para serializar/merge)
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

#endif // COMMON_HASH_H
