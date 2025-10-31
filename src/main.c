#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strcmp, strncmp, strlen, strdup, strtok
#include <unistd.h> // For access() and X_OK

#define MAX_COMMAND_LENGTH 1024
#define MAX_PATH_LENGTH 1024

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
      printf("%s\n", command + 5);
      continue;
    }

    // 6. Check for 'type' command
    if (strncmp(command, "type ", 5) == 0)
    {
      char *arg = command + 5;

      // Step 6a: Check for builtins first
      if (strcmp(arg, "echo") == 0 ||
          strcmp(arg, "exit") == 0 ||
          strcmp(arg, "type") == 0)
      {
        printf("%s is a shell builtin\n", arg);
        continue; // Command handled
      }

      // Step 6b: If not a builtin, search PATH
      char *path_env = getenv("PATH"); // Get the PATH variable
      if (path_env == NULL)
      {
        // Should not happen in test, but good practice
        printf("%s: not found\n", arg);
        continue;
      }

      // We must *copy* path_env, because strtok() modifies the string
      char *path_copy = strdup(path_env);
      if (path_copy == NULL)
      {
        // Memory allocation failed
        fprintf(stderr, "Failed to allocate memory\n");
        continue;
      }

      char *dir = strtok(path_copy, ":"); // Split by colon
      int found = 0;
      char full_path[MAX_PATH_LENGTH];

      while (dir != NULL)
      {
        // Construct the full path: dir + "/" + arg
        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", dir, arg);

        // Check if file exists AND has execute permissions (X_OK)
        if (access(full_path, X_OK) == 0)
        {
          printf("%s is %s\n", arg, full_path);
          found = 1;
          break; // Stop searching once found
        }

        dir = strtok(NULL, ":"); // Get the next directory
      }

      free(path_copy); // Clean up the duplicated string

      if (!found)
      {
        printf("%s: not found\n", arg);
      }

      continue; // Command handled
    }

    // 7. Handle empty input
    if (strlen(command) == 0)
    {
      continue;
    }

    // 8. Print the error message (if no other command matched)
    fprintf(stderr, "%s: command not found\n", command);
  }

  return 0;
}