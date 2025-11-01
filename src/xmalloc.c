#include "xmalloc.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        err(EXIT_FAILURE, "malloc");
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (ptr == NULL) {
        err(EXIT_FAILURE, "realloc");
    }
    return ptr;
}

char *xstrdup(const char *s1) {
    char *s2 = strdup(s1);
    if (s2 == NULL) {
        err(EXIT_FAILURE, "strdup");
    }
    return s2;
}

char *xstrndup(const char *s1, size_t n) {
    char *s2 = strndup(s1, n);
    if (s2 == NULL) {
        err(EXIT_FAILURE, "strndup");
    }
    return s2;
}
