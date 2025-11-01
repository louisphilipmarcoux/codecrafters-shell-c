#include "cmd.h"
#include "misc.h"
#include "ptr_array.h"
#include "redir.h"
#include "xmalloc.h"

#include <err.h>
#include <errno.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void cmd_cd(const PtrArray *arguments) {
    const char *dir = ptr_array_get_const(arguments, 1);
    if (chdir(dir) < 0) {
        fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
    }
}

static void cmd_echo(const PtrArray *arguments) {
    size_t num_args = ptr_array_get_size(arguments);
    for (size_t i = 1; i < num_args; i++) {
        if (i > 1) {
            printf(" ");
        }
        printf("%s", (const char *)ptr_array_get_const(arguments, i));
    }
    printf("\n");
}

static void cmd_exit(const PtrArray *arguments) {
    int status = atoi((const char *)ptr_array_get_const(arguments, 1));
    exit(status);
}

static void cmd_history(const PtrArray *arguments) {
    static int last_append_n = 0;
    int n = history_length + 1 - history_base;

    if (ptr_array_get_size(arguments) > 2) {
        const char *option = ptr_array_get_const(arguments, 1);
        const char *histfile = ptr_array_get_const(arguments, 2);

        if (strcmp(option, "-r") == 0) {
            read_history(histfile);
        } else if (strcmp(option, "-w") == 0) {
            write_history(histfile);
        } else if (strcmp(option, "-a") == 0) {
            append_history(n - last_append_n, histfile);
            last_append_n = n;
        }
        return;
    }

    if (ptr_array_get_size(arguments) > 1) {
        n = atoi((const char *)ptr_array_get_const(arguments, 1));
    }

    for (int i = history_length + 1 - n; i <= history_length; i++) {
        HIST_ENTRY *entry = history_get(i);
        printf("%5d  %s\n", i, entry->line);
    }
}

static void cmd_pwd(const PtrArray *arguments) {
    char *cwd = getcwd(NULL, 0);
    printf("%s\n", cwd);
    free(cwd);
}

static void cmd_type(const PtrArray *arguments) {
    size_t num_args = ptr_array_get_size(arguments);
    for (size_t i = 1; i < num_args; i++) {
        const char *name = ptr_array_get_const(arguments, i);

        if (is_builtin(name)) {
            printf("%s is a shell builtin\n", name);
            continue;
        }

        char *path = find_executable(name);
        if (path != NULL) {
            printf("%s is %s\n", name, path);
            free(path);
            continue;
        }

        printf("%s: not found\n", name);
    }
}

static void execute_builtin(const PtrArray *arguments) {
    const char *cmd_name = ptr_array_get_const(arguments, 0);
    if (strcmp(cmd_name, "cd") == 0) {
        cmd_cd(arguments);
    } else if (strcmp(cmd_name, "echo") == 0) {
        cmd_echo(arguments);
    } else if (strcmp(cmd_name, "exit") == 0) {
        cmd_exit(arguments);
    } else if (strcmp(cmd_name, "history") == 0) {
        cmd_history(arguments);
    } else if (strcmp(cmd_name, "pwd") == 0) {
        cmd_pwd(arguments);
    } else if (strcmp(cmd_name, "type") == 0) {
        cmd_type(arguments);
    }
}

__attribute__((noreturn))
static void execute_external(const char *path, PtrArray *arguments) {
    ptr_array_append(arguments, NULL);
    execvp(path, (char **)ptr_array_get_c_array(arguments));
    err(EXIT_FAILURE, "execvp");
}

struct Cmd {
    PtrArray *arguments;
    PtrArray *redirs;
};

static void execute(Cmd *cmd, bool fork_on_external) {
    size_t num_redirs = ptr_array_get_size(cmd->redirs);
    for (size_t i = 0; i < num_redirs; i++) {
        redir_do((Redir *)ptr_array_get(cmd->redirs, i));
    }

    const char *cmd_name = ptr_array_get(cmd->arguments, 0);
    if (is_builtin(cmd_name)) {
        execute_builtin(cmd->arguments);
    } else {
        char *path = find_executable(cmd_name);
        if (path == NULL) {
            fprintf(stderr, "%s: command not found\n", cmd_name);
            return;
        }

        if (!fork_on_external || fork() == 0) {
            execute_external(path, cmd->arguments);
        }
        wait(NULL);
        free(path);
    }

    for (size_t i = 0; i < num_redirs; i++) {
        redir_undo((Redir *)ptr_array_get(cmd->redirs, num_redirs - 1 - i));
    }
}

Cmd *cmd_create(PtrArray *arguments, PtrArray *redirs) {
    Cmd *cmd = xmalloc(sizeof(Cmd));
    cmd->arguments = arguments;
    cmd->redirs = redirs;
    return cmd;
}

void cmd_destroy(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    Cmd *cmd = ptr;
    ptr_array_destroy(cmd->arguments, free);
    ptr_array_destroy(cmd->redirs, redir_destroy);
    free(cmd);
}

void execute_cmds(PtrArray *cmds) {
    size_t num_cmds = ptr_array_get_size(cmds);
    if (num_cmds == 0) {
        return;
    } else if (num_cmds == 1) {
        execute((Cmd *)ptr_array_get(cmds, 0), true);
        return;
    }

    int fds[2], prev_rfd;

    for (size_t i = 0; i < num_cmds; i++) {
        if (i < num_cmds - 1) {
            pipe(fds);
        }

        if (fork() == 0) {
            if (i > 0) {
                dup2(prev_rfd, STDIN_FILENO);
                close(prev_rfd);
            }
            if (i < num_cmds - 1) {
                dup2(fds[1], STDOUT_FILENO);
                close(fds[1]);
            }

            execute((Cmd *)ptr_array_get(cmds, i), false);
            exit(EXIT_SUCCESS);
        }

        if (i > 0) {
            close(prev_rfd);
        }
        if (i < num_cmds - 1) {
            close(fds[1]);
        }

        prev_rfd = fds[0];
    }

    for (size_t i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
}
