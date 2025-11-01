#include "trie.h"
#include "ptr_array.h"
#include "xmalloc.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TrieNode {
    struct TrieNode *children[CHAR_MAX];
    bool has_value;
} TrieNode;

static TrieNode *trie_node_create(void) {
    TrieNode *node = xmalloc(sizeof(TrieNode));
    for (char c = 0; c < CHAR_MAX; c++) {
        node->children[c] = NULL;
    }
    node->has_value = false;
    return node;
}

static void trie_node_destroy(TrieNode *node) {
    if (node == NULL) {
        return;
    }
    for (char c = 0; c < CHAR_MAX; c++) {
        trie_node_destroy(node->children[c]);
    }
    free(node);
}

struct Trie {
    TrieNode *root;
};

Trie *trie_create(void) {
    Trie *trie = xmalloc(sizeof(Trie));
    trie->root = trie_node_create();
    return trie;
}

void trie_destroy(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    Trie *trie = ptr;
    trie_node_destroy(trie->root);
    free(trie);
}

void trie_insert(Trie *trie, const char *str) {
    TrieNode *current = trie->root;
    for (const char *p = str; *p != '\0'; p++) {
        char c = *p;
        if (current->children[c] == NULL) {
            current->children[c] = trie_node_create();
        }
        current = current->children[c];
    }
    current->has_value = true;
}

static const TrieNode *locate(const Trie *trie, const char *str) {
    const TrieNode *current = trie->root;
    for (const char *p = str; *p != '\0'; p++) {
        char c = *p;
        if (current->children[c] == NULL) {
            return NULL;
        }
        current = current->children[c];
    }
    return current;
}

bool trie_search(const Trie *trie, const char *str) {
    const TrieNode *node = locate(trie, str);
    return node != NULL && node->has_value;
}

static void collect_strings(const TrieNode *node, char *const str, size_t i, PtrArray *candidates) {
    if (node->has_value) {
        str[i] = '\0';
        ptr_array_append(candidates, xstrdup(str));
    }

    for (char c = 0; c < CHAR_MAX; c++) {
        if (node->children[c] != NULL) {
            str[i] = c;
            collect_strings(node->children[c], str, i + 1, candidates);
        }
    }
}

PtrArray *trie_autocmp(const Trie *trie, const char *prefix) {
    PtrArray *candidates = ptr_array_create();
    const TrieNode *node = locate(trie, prefix);
    if (node != NULL) {
        char str[4096];
        snprintf(str, sizeof(str), "%s", prefix);
        collect_strings(node, str, strlen(prefix), candidates);
    }
    return candidates;
}
