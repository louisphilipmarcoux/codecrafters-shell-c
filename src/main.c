#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Need this for strcmp() and strlen()

#define MAX_COMMAND_LENGTH 1024

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
    if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL)
    {
      break; // End of file (Ctrl+D)
    }

    // 3. Clean up the input (remove the newline character)
    size_t len = strlen(command);
    if (len > 0 && command[len - 1] == '\n')
    {
      command[len - 1] = '\0';
    }

    // 4. Check for the 'exit 0' command
    // strcmp returns 0 if the strings are identical.
    if (strcmp(command, "exit 0") == 0)
    {
      // Exit the shell with status 0
      // Returning 0 from main() is equivalent to exit(0)
      return 0;
    }

    // 5. Handle empty input
    if (strlen(command) == 0)
    {
      continue;
    }

    // 6. Print the error message (if not exit)
    fprintf(stderr, "%s: command not found\n", command);
  }

  return 0;
}