#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // For strcmp, strncmp, strlen, strdup, strtok
#include <unistd.h>    // For access(), X_OK, fork(), execv()
#include <sys/wait.h>  // For waitpid()
#include <sys/types.h> // For pid_t

#define MAX_COMMAND_LENGTH 1024
#define MAX_PATH_LENGTH 1024
#define MAX_ARGS 64 // Max number of arguments for a command

/**
 * Helper function to find an executable in PATH.
 * Populates 'full_path' if found.
 * Returns 1 if found, 0 otherwise.
 */
int find_executable(const char *command, char *full_path, size_t full_path_size)
{
  char *path_env = getenv("PATH");
  if (path_env == NULL)
    return 0;

  char *path_copy = strdup(path_env);
  if (path_copy == NULL)
  {
    perror("strdup");
    return 0;
  }

  char *dir = strtok(path_copy, ":");
  while (dir != NULL)
  {
    // Construct the full path
    snprintf(full_path, full_path_size, "%s/%s", dir, command);

    // Check if file exists and has execute permissions
    if (access(full_path, X_OK) == 0)
    {
      free(path_copy);
      return 1; // Found
    }
    dir = strtok(NULL, ":");
  }

  free(path_copy);
  return 0; // Not found
}

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

    // 4. Handle empty input
    if (strlen(command) == 0)
    {
      continue;
    }

    // --- Builtin Command Handling ---

    // 5. Check for 'exit 0'
    if (strcmp(command, "exit 0") == 0)
    {
      return 0; // Exit the shell
    }

    // 6. Check for 'echo' command
    if (strncmp(command, "echo ", 5) == 0)
    {
      printf("%s\n", command + 5);
      continue;
    }

    // 7. Check for 'type' command
    if (strncmp(command, "type ", 5) == 0)
    {
      char *arg = command + 5;

      // Check builtins
      if (strcmp(arg, "echo") == 0 || strcmp(arg, "exit") == 0 || strcmp(arg, "type") == 0)
      {
        printf("%s is a shell builtin\n", arg);
      }
      else
      {
        // Check executable in PATH
        char full_path[MAX_PATH_LENGTH];
        if (find_executable(arg, full_path, MAX_PATH_LENGTH))
        {
          printf("%s is %s\n", arg, full_path);
        }
        else
        {
          printf("%s: not found\n", arg);
        }
      }
      continue; // Command handled
    }

    // --- External Command Execution ---
    // If it wasn't a builtin, try to execute it.

    // 8. Parse the command and its arguments
    char *args[MAX_ARGS];
    int i = 0;
    // strtok modifies the string, which is fine now since builtins are done.
    char *token = strtok(command, " ");

    while (token != NULL)
    {
      args[i++] = token;
      if (i >= MAX_ARGS - 1)
      {        // -1 to leave space for NULL
        break; // Too many args
      }
      token = strtok(NULL, " ");
    }
    args[i] = NULL; // The args array must be NULL-terminated for execv

    char *cmd_name = args[0];
    char full_path[MAX_PATH_LENGTH];

    // 9. Find the executable
    if (find_executable(cmd_name, full_path, MAX_PATH_LENGTH))
    {

      // 10. Fork, Exec, Wait
      pid_t pid = fork();

      if (pid == -1)
      {
        // Fork failed
        perror("fork");
      }
      else if (pid == 0)
      {
        // --- This is the Child Process ---
        // execv replaces this child process with the new program
        if (execv(full_path, args) == -1)
        {
          // execv only returns if an error occurred
          perror("execv");
          exit(EXIT_FAILURE); // Exit child process if exec fails
        }
      }
      else
      {
        // --- This is the Parent Process (your shell) ---
        // Wait for the child process to finish
        int status;
        waitpid(pid, &status, 0);
      }
    }
    else
    {
      // 11. If not found, print error
      fprintf(stderr, "%s: command not found\n", cmd_name);
    }
  }

  return 0;
}