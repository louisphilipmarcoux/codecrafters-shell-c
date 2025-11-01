#include "parse.h"
#include "cmd.h"
#include "ptr_array.h"
#include "redir.h"
#include "token.h"
#include "xmalloc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>

static struct {
    const PtrArray *tokens;
    size_t current;
    PtrArray *cmds;
} parser;

static void init(const PtrArray *tokens) {
    parser.tokens = tokens;
    parser.current = 0;
    parser.cmds = ptr_array_create();
}

static const Token *peek(void) {
    return ptr_array_get_const(parser.tokens, parser.current);
}

static bool check(TokenType type) {
    return peek()->type == type;
}

static bool is_at_end(void) {
    return check(TOKEN_EOF);
}

static const Token *previous(void) {
    return ptr_array_get_const(parser.tokens, parser.current - 1);
}

static const Token *advance(void) {
    if (!is_at_end()) {
        parser.current++;
    }
    return previous();
}

static bool match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

static Cmd *command(void) {
    PtrArray *arguments = ptr_array_create();
    PtrArray *redirs = ptr_array_create();

    while (!is_at_end() && !check(TOKEN_OR)) {
        if (match(TOKEN_WORD)) {
            static wordexp_t we;
            wordexp(previous()->lexeme, &we, WRDE_REUSE);
            for (size_t i = 0; i < we.we_wordc; i++) {
                ptr_array_append(arguments, xstrdup(we.we_wordv[i]));
            }
            continue;
        }

        int fd = STDOUT_FILENO;
        if (match(TOKEN_IO_NUMBER)) {
            fd = atoi(previous()->lexeme);
        }
        assert(match(TOKEN_DGREAT) || match(TOKEN_GREAT));
        RedirMode mode = previous()->type == TOKEN_DGREAT ? REDIR_APPEND : REDIR_NORMAL;
        const char *path = advance()->lexeme;

        ptr_array_append(redirs, redir_create(fd, path, mode));
    }

    return cmd_create(arguments, redirs);
}

PtrArray *parse(const PtrArray *tokens) {
    init(tokens);
    if (!is_at_end()) {
        do {
            ptr_array_append(parser.cmds, command());
        } while (match(TOKEN_OR));
    }
    return parser.cmds;
}
