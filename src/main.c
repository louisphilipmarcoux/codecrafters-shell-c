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

// Define parser states
enum
{
  STATE_DEFAULT,
  STATE_IN_QUOTE, // Single quote
  STATE_IN_DQUOTE // Double quote
};

/**
 * Helper function to find an executable in PATH.
 * ... (This function is unchanged) ...
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
    // ... (Builtin handling is unchanged) ...

    // 5. Check for 'exit 0'
    if (strcmp(command, "exit 0") == 0)
    {
      return 0;
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
      continue;
    }

    // 7. 'echo' builtin is removed

    // 8. Check for 'cd'
    if (strncmp(command, "cd ", 3) == 0)
    {
      char *path_arg = command + 3;
      char *path_to_change = NULL;

      if (strcmp(path_arg, "~") == 0)
      {
        path_to_change = getenv("HOME");
        if (path_to_change == NULL)
        {
          fprintf(stderr, "cd: HOME not set\n");
          continue;
        }
      }
      else
      {
        path_to_change = path_arg;
      }

      if (chdir(path_to_change) != 0)
      {
        fprintf(stderr, "cd: %s: %s\n", path_arg, strerror(errno));
      }
      continue;
    }

    // 9. Check for 'type'
    if (strncmp(command, "type ", 5) == 0)
    {
      char *arg = command + 5;

      if (strcmp(arg, "echo") == 0 || strcmp(arg, "exit") == 0 ||
          strcmp(arg, "type") == 0 || strcmp(arg, "pwd") == 0 ||
          strcmp(arg, "cd") == 0)
      {
        printf("%s is a shell builtin\n", arg);
      }
      else
      {
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
      continue;
    }

    // --- External Command Execution (UPDATED PARSER) ---

    // 10. Parse the command and its arguments (with quote handling)
    char *args[MAX_ARGS];
    int arg_index = 0;

    char *read_ptr = command;  // Read from the original command string
    char *write_ptr = command; // Write the parsed argument back into the same string

    int state = STATE_DEFAULT;
    args[arg_index] = write_ptr; // The start of the first argument
    int new_arg = 1;             // Flag to indicate if we're at the start of a new arg

    while (*read_ptr != '\0')
    {
      char c = *read_ptr;

      if (state == STATE_DEFAULT)
      {
        // NEW: Check for backslash escape *first*
        if (c == '\\')
        {
          read_ptr++; // Consume backslash
          if (*read_ptr == '\0')
            break;                // Dangling backslash
          *write_ptr = *read_ptr; // Copy escaped char literally
          write_ptr++;
          read_ptr++; // Consume escaped char
          new_arg = 0;
        }
        else if (c == ' ')
        {
          // Space delimiter
          if (!new_arg)
          {                    // Only end arg if we've written something to it
            *write_ptr = '\0'; // Null-terminate the current argument
            write_ptr++;

            // Start next argument
            arg_index++;
            if (arg_index >= MAX_ARGS - 1)
            {
              fprintf(stderr, "Error: Too many arguments\n");
              break;
            }
            args[arg_index] = write_ptr;
            new_arg = 1;
          }
          read_ptr++; // Skip the space
        }
        else if (c == '\'')
        {
          // Start of single quote
          state = STATE_IN_QUOTE;
          read_ptr++;  // Don't copy the quote
          new_arg = 0; // We are now writing to an argument
        }
        else if (c == '"')
        {
          // Start of double quote
          state = STATE_IN_DQUOTE;
          read_ptr++;  // Don't copy the quote
          new_arg = 0; // We are now writing to an argument
        }
        else
        {
          // Regular character
          *write_ptr = c;
          write_ptr++;
          read_ptr++;
          new_arg = 0; // We are now writing to an argument
        }
      }
      else if (state == STATE_IN_QUOTE)
      {
        if (c == '\'')
        {
          // End of single quote
          state = STATE_DEFAULT;
          read_ptr++; // Don't copy the quote
        }
        else
        {
          // Literal character (backslashes are literal in single quotes)
          *write_ptr = c;
          write_ptr++;
          read_ptr++;
        }
      }
      else if (state == STATE_IN_DQUOTE)
      {
        // NEW: Handle backslash inside double quotes
        if (c == '\\')
        {
          // Only escape \ and " (for this stage)
          if (read_ptr[1] == '\\' || read_ptr[1] == '"')
          {
            read_ptr++;             // Consume backslash
            *write_ptr = *read_ptr; // Copy escaped \ or "
            write_ptr++;
            read_ptr++; // Consume escaped char
          }
          else
          {
            // Not escaping a special char, so treat \ literally
            *write_ptr = c;
            write_ptr++;
            read_ptr++;
          }
        }
        else if (c == '"')
        {
          // End of double quote
          state = STATE_DEFAULT;
          read_ptr++; // Don't copy the quote
        }
        else
        {
          // Literal character
          *write_ptr = c;
          write_ptr++;
          read_ptr++;
        }
      }
    } // end while

    // Updated error handling for unclosed quotes
    if (state == STATE_IN_QUOTE)
    {
      fprintf(stderr, "Error: Unclosed single quote\n");
      continue;
    }
    if (state == STATE_IN_DQUOTE)
    {
      fprintf(stderr, "Error: Unclosed double quote\n");
      continue;
    }

    *write_ptr = '\0'; // Null-terminate the last argument

    // Set the next arg pointer to NULL for execv
    if (!new_arg)
    { // If we were in the middle of writing an arg
      arg_index++;
    }
    args[arg_index] = NULL;

    // 11. Check for empty command (e.g., just spaces or empty quotes)
    if (args[0] == NULL)
    {
      continue;
    }

    // --- (Execution logic is unchanged) ---
    char *cmd_name = args[0];
    char full_path[MAX_PATH_LENGTH];

    // 12. Find and execute
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
      // 13. If not found, print error
      fprintf(stderr, "%s: command not found\n", cmd_name);
    }
  }

  return 0;
}