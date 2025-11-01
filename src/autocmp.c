#include "autocmp.h"
#include "misc.h"
#include "ptr_array.h"
#include "trie.h"
#include "xmalloc.h"

#include <readline/readline.h>
#include <stdlib.h>

static Trie *trie = NULL;

static void add_names(const PtrArray *names) {
    size_t num_names = ptr_array_get_size(names);
    for (size_t i = 0; i < num_names; i++) {
        const char *name = ptr_array_get_const(names, i);
        trie_insert(trie, name);
    }
}

static void init_trie(void) {
    trie = trie_create();
    add_names(get_all_builtin_names());
    add_names(get_all_executable_names());
}

static char *shell_completion_generator(const char *text, int state) {
    if (trie == NULL) {
        init_trie();
    }

    static PtrArray *candidates = NULL;
    static size_t index = 0;
    if (state == 0) {
        if (candidates != NULL) {
            ptr_array_destroy(candidates, free);
        }
        candidates = trie_autocmp(trie, text);
        index = 0;
    }

    if (index >= ptr_array_get_size(candidates)) {
        return NULL;
    }
    return xstrdup((const char *)ptr_array_get_const(candidates, index++));
}

char **shell_completion(const char *text, int start, int end) {
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, shell_completion_generator);
}
