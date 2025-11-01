#ifndef CODECRAFTERS_SHELL_CMD_H_INCLUDED
#define CODECRAFTERS_SHELL_CMD_H_INCLUDED

#include "ptr_array.h"

typedef struct Cmd Cmd;

// Allocates memory for a command.
Cmd *cmd_create(PtrArray *arguments, PtrArray *redirs);

// Deallocates memory for a command.
void cmd_destroy(void *cmd);

// Executes one or more commands. In the case of multiple commands, a pipeline is created to
// transmit input/output between them.
void execute_cmds(PtrArray *cmds);

#endif
