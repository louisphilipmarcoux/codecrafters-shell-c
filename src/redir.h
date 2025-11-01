#ifndef CODECRAFTERS_SHELL_REDIR_H_INCLUDED
#define CODECRAFTERS_SHELL_REDIR_H_INCLUDED

typedef enum {
    REDIR_NORMAL,
    REDIR_APPEND,
} RedirMode;

typedef struct Redir Redir;

// Allocates memory for an IO redirection.
Redir *redir_create(int fd, const char *path, RedirMode mode);

// Deallocates memory for an IO redirection.
void redir_destroy(void *redir);

// Does an IO redirection.
void redir_do(Redir *redir);

// Undoes an IO redirection.
void redir_undo(Redir *redir);

#endif
