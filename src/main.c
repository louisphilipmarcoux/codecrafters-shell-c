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
    if (strncmp(command, "echo ", 5) == 0)
    {
      // 'command + 5' is a pointer to the character *after* "echo "
      printf("%s\n", command + 5);
      continue; // Command handled, loop back
    }

    // 6. NEW: Check for 'type' command
    if (strncmp(command, "type ", 5) == 0)
    {
      // Get the argument *after* "type "
      char *arg = command + 5;

      // Check if the argument is one of the known builtins
      if (strcmp(arg, "echo") == 0 ||
          strcmp(arg, "exit") == 0 ||
          strcmp(arg, "type") == 0)
      {
        printf("%s is a shell builtin\n", arg);
      }
      else
      {
        // If not a known builtin, print "not found"
        printf("%s: not found\n", arg);
      }

      continue; // Command handled, loop back
    }

    // 7. Handle empty input
    if (strlen(command) == 0)
    {
      continue;
    }

    // 8. Print the error message (if no other command matched)
    // Note: This is for *executing* a command, not for 'type'
    fprintf(stderr, "%s: command not found\n", command);
  }

  return 0;
}