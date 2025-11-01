#include "token.h"
#include "xmalloc.h"

#include <stdlib.h>

Token *token_create(TokenType type, char *lexeme) {
    Token *token = xmalloc(sizeof(Token));
    token->type = type;
    token->lexeme = lexeme;
    return token;
}

void token_destroy(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    Token *token = ptr;
    free(token->lexeme);
    free(token);
}
