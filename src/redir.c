#include "redir.h"
#include "xmalloc.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

struct Redir {
    int fd, saved_fd;
    char *path;
    RedirMode mode;
};

Redir *redir_create(int fd, const char *path, RedirMode mode) {
    Redir *redir = xmalloc(sizeof(Redir));
    redir->fd = fd;
    redir->path = xstrdup(path);
    redir->mode = mode;
    return redir;
}

void redir_destroy(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    Redir *redir = ptr;
    free(redir->path);
    free(redir);
}

void redir_do(Redir *redir) {
    int open_flags = O_WRONLY | O_CREAT | (redir->mode == REDIR_APPEND ? O_APPEND : O_TRUNC);
    int file_fd = open(redir->path, open_flags, 0644);
    redir->saved_fd = dup(redir->fd);
    dup2(file_fd, redir->fd);
    close(file_fd);
}

void redir_undo(Redir *redir) {
    fsync(redir->fd);
    dup2(redir->saved_fd, redir->fd);
    close(redir->saved_fd);
}
