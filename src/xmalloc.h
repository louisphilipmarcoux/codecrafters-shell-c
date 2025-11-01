#ifndef CODECRAFTERS_SHELL_XMALLOC_H_INCLUDED
#define CODECRAFTERS_SHELL_XMALLOC_H_INCLUDED

#include <stddef.h>

// Allocates memory. Exits on error.
void *xmalloc(size_t size);

// Reallocates memory. Exits on error.
void *xrealloc(void *ptr, size_t size);

// Duplicates a string. Exits on error.
char *xstrdup(const char *s1);

// Copies at most n characters from a string. Exits on error.
char *xstrndup(const char *s1, size_t n);

#endif
