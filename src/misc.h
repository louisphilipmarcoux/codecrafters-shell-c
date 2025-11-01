#ifndef CODECRAFTERS_SHELL_MISC_H_INCLUDED
#define CODECRAFTERS_SHELL_MISC_H_INCLUDED

#include <stdbool.h>

#include "ptr_array.h"

// Returns an array of builtin command names.
const PtrArray *get_all_builtin_names(void);

// Checks whether a command name is a builtin.
bool is_builtin(const char *name);

// Finds an executable of the given name under the PATH environment variable. Returns a dynamically
// allocated path to the found executable, or NULL if not found.
char *find_executable(const char *name);

// Returns an array of names of all executables under the PATH environment variable.
const PtrArray *get_all_executable_names(void);

#endif
