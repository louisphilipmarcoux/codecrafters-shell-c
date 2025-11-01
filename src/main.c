#include <readline/history.h>
#include <readline/readline.h>
#include <stdlib.h>

#include "autocmp.h"
#include "cmd.h"
#include "parse.h"
#include "ptr_array.h"
#include "scan.h"
#include "token.h"

static void write_history_file(void) {
    const char *histfile = getenv("HISTFILE");
    if (histfile != NULL) {
        write_history(histfile);
    }
}

static void setup(void) {
    rl_attempted_completion_function = shell_completion;

    using_history();
    const char *histfile = getenv("HISTFILE");
    if (histfile != NULL) {
        read_history(histfile);
    }
    atexit(write_history_file);
}

static PtrArray *parse_line_to_cmds(const char *line) {
    PtrArray *tokens = scan(line);
    PtrArray *cmds = parse(tokens);
    ptr_array_destroy(tokens, token_destroy);
    return cmds;
}

int main(void) {
    setup();

    char *line;
    while ( (line = readline("$ ")) != NULL) {
        add_history(line);
        PtrArray *cmds = parse_line_to_cmds(line);
        free(line);
        execute_cmds(cmds);
        ptr_array_destroy(cmds, cmd_destroy);
    }

    exit(EXIT_SUCCESS);
}
