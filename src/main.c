#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for string manipulation, though technically not needed for the *simplest* read.

#define MAX_COMMAND_LENGTH 1024 // Define a maximum size for the command buffer

int main(int argc, char *argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL);

  while (1)
  {
    // 1. Print the prompt
    printf("$ ");

    // 2. Read the user's input (command)
    char command[MAX_COMMAND_LENGTH];

    // Check if reading the line was successful
    if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL)
    {
      // End of file (Ctrl+D) or an error occurred
      break;
    }

    // 3. Clean up the input (remove the newline character)
    // fgets includes the newline character, which we need to remove for clean output.
    size_t len = strlen(command);
    if (len > 0 && command[len - 1] == '\n')
    {
      command[len - 1] = '\0';
    }

    // 4. Handle empty input (if the user just pressed Enter)
    if (strlen(command) == 0)
    {
      continue;
    }

    // 5. Print the error message
    // Format: <command>: command not found
    // Since we're treating ALL commands as invalid for this stage, we always print this.
    fprintf(stderr, "%s: command not found\n", command);
  }

  return 0;
}