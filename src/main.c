#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // For strcmp, strncmp, strlen, strdup, strtok, strerror
#include <unistd.h>    // For access(), X_OK, fork(), execv(), getcwd(), chdir()
#include <sys/wait.h>  // For waitpid()
#include <sys/types.h> // For pid_t
#include <errno.h>     // For errno (used with chdir)

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
    snprintf(full_path, full_path_size, "%s/%s", dir, command);
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

    // 6. Check for 'pwd'
    if (strcmp(command, "pwd") == 0)
    {
      char cwd_buffer[MAX_PATH_LENGTH];
      if (getcwd(cwd_buffer, sizeof(cwd_buffer)) != NULL)
      {
        printf("%s\n", cwd_buffer);
      }
      else
      {
        perror("getcwd");
      }
      continue; // Command handled
    }

    // 7. Check for 'echo' command
    if (strncmp(command, "echo ", 5) == 0)
    {
      printf("%s\n", command + 5);
      continue;
    }

    // 8. UPDATED: Check for 'cd' command
    if (strncmp(command, "cd ", 3) == 0)
    {
      // Get the argument (e.g., "/usr" or "~")
      char *path_arg = command + 3;
      char *path_to_change = NULL;

      // Check if the argument is exactly "~"
      if (strcmp(path_arg, "~") == 0)
      {
        path_to_change = getenv("HOME");
        if (path_to_change == NULL)
        {
          fprintf(stderr, "cd: HOME not set\n");
          continue; // Skip chdir
        }
      }
      else
      {
        // Otherwise, use the argument as given
        path_to_change = path_arg;
      }

      // Attempt to change directory
      if (chdir(path_to_change) != 0)
      {
        // On failure, print the *original* argument
        fprintf(stderr, "cd: %s: %s\n", path_arg, strerror(errno));
      }

      continue;
    }

    // 9. Check for 'type' command
    if (strncmp(command, "type ", 5) == 0)
    {
      char *arg = command + 5;

      // Check builtins
      if (strcmp(arg, "echo") == 0 || strcmp(arg, "exit") == 0 ||
          strcmp(arg, "type") == 0 || strcmp(arg, "pwd") == 0 ||
          strcmp(arg, "cd") == 0)
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
    // ... (rest of the code is unchanged) ...

    // 10. Parse the command and its arguments
    char *args[MAX_ARGS];
    int i = 0;
    // We need a mutable copy of the command for strtok
    char command_copy[MAX_COMMAND_LENGTH];
    strcpy(command_copy, command);

    char *token = strtok(command_copy, " ");

    while (token != NULL)
    {
      args[i++] = token;
      if (i >= MAX_ARGS - 1)
      {
        break;
      }
      token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (args[0] == NULL)
    { // Handle case like " " (spaces only)
      continue;
    }

    char *cmd_name = args[0];
    char full_path[MAX_PATH_LENGTH];

    // 11. Find the executable
    if (find_executable(cmd_name, full_path, MAX_PATH_LENGTH))
    {

      pid_t pid = fork();

      if (pid == -1)
      {
        perror("fork");
      }
      else if (pid == 0)
      {
        // Child Process
        if (execv(full_path, args) == -1)
        {
          perror("execv");
          exit(EXIT_FAILURE);
        }
      }
      else
      {
        // Parent Process
        int status;
        waitpid(pid, &status, 0);
      }
    }
    else
    {
      // 12. If not found, print error
      fprintf(stderr, "%s: command not found\n", cmd_name);
    }
  }

  return 0;
}