#include "scan.h"
#include "ptr_array.h"
#include "token.h"
#include "xmalloc.h"

#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>

static struct {
    const char *start;
    const char *current;
    PtrArray *tokens;
} scanner;

static void init(const char *line) {
    scanner.start = line;
    scanner.current = line;
    scanner.tokens = ptr_array_create();
}

static char peek(void) {
    return *scanner.current;
}

static bool is_at_end(void) {
    return peek() == '\0';
}

static char advance(void) {
    return *scanner.current++;
}

static bool match(char expected) {
    if (peek() != expected) {
        return false;
    }
    advance();
    return true;
}

static char *get_lexeme(void) {
    size_t lexeme_length = scanner.current - scanner.start;
    return xstrndup(scanner.start, lexeme_length);
}

static void add_token(TokenType type) {
    Token *token = token_create(type, get_lexeme());
    ptr_array_append(scanner.tokens, token);
}

static void single_quote(void) {
    while (!is_at_end() && peek() != '\'') {
        advance();
    }
    if (is_at_end()) {
        errx(EXIT_FAILURE, "missing single quote");
    }
    advance();
}

static void double_quote(void) {
    while (!is_at_end() && peek() != '\"') {
        if (advance() == '\\') {
            if (is_at_end()) {
                errx(EXIT_FAILURE, "expected character after backslash");
            }
            advance();
        }
    }
    if (is_at_end()) {
        errx(EXIT_FAILURE, "missing double quote");
    }
    advance();
}

static void word(void) {
    while (!is_at_end() && !isspace(peek())) {
        switch (advance()) {
            case '\'':
                single_quote();
                break;
            case '\"':
                double_quote();
                break;
            case '\\':
                if (is_at_end()) {
                    errx(EXIT_FAILURE, "expected character after backslash");
                }
                advance();
                break;
            default:
                break;
        }
    }
    add_token(TOKEN_WORD);
}

static void number(void) {
    while (!is_at_end() && isdigit(peek())) {
        advance();
    }

    if (peek() == '>') {
        add_token(TOKEN_IO_NUMBER);
    } else {
        word();
    }
}

static void scan_token(void) {
    char c = peek();
    switch (c) {
        case '|':
            advance();
            add_token(TOKEN_OR);
            break;
        case ' ':
            advance();
            break;
        case '>':
            advance();
            add_token(match('>') ? TOKEN_DGREAT : TOKEN_GREAT);
            break;
        default:
            if (isdigit(c)) {
                number();
            } else {
                word();
            }
            break;
    }
}

PtrArray *scan(const char *line) {
    init(line);
    while (!is_at_end()) {
        scan_token();
        scanner.start = scanner.current;
    }
    add_token(TOKEN_EOF);
    return scanner.tokens;
}
