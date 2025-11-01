#ifndef CODECRAFTERS_SHELL_TRIE_H_INCLUDED
#define CODECRAFTERS_SHELL_TRIE_H_INCLUDED

#include "ptr_array.h"

typedef struct Trie Trie;

// Allocates memory for an empty trie.
Trie *trie_create(void);

// Deallocates memory for a trie and its strings.
void trie_destroy(void *trie);

// Inserts a string into a trie.
void trie_insert(Trie *trie, const char *str);

// Searches for a complete string in the trie.
bool trie_search(const Trie *trie, const char *str);

// Returns an array of strings in a trie that start with a given prefix.
PtrArray *trie_autocmp(const Trie *trie, const char *prefix);

#endif
