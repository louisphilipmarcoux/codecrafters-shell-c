#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Need this for strcmp(), strlen(), and strncmp()

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

    // --- Command Handling ---

    // 4. Check for 'exit 0'
    if (strcmp(command, "exit 0") == 0)
    {
      return 0; // Exit the shell
    }

    // 5. Check for 'echo' command
    // We use strncmp to check if the command *starts with* "echo "
    // The " " (space) is important! "echo" has 4 chars, "echo " has 5.
    if (strncmp(command, "echo ", 5) == 0)
    {
      // If it matches, print the rest of the string
      // 'command + 5' is a pointer to the character *after* "echo "
      printf("%s\n", command + 5);

      // We've handled the command, so loop back to the prompt
      continue;
    }

    // 6. Handle empty input
    if (strlen(command) == 0)
    {
      continue;
    }

    // 7. Print the error message (if no other command matched)
    fprintf(stderr, "%s: command not found\n", command);
  }

  return 0;
}