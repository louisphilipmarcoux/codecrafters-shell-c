#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // For strcmp, strncmp, strlen, strdup, strtok, strerror
#include <unistd.h>    // For access(), X_OK, fork(), execv(), getcwd(), chdir(), dup2(), close()
#include <sys/wait.h>  // For waitpid()
#include <sys/types.h> // For pid_t
#include <errno.h>     // For errno (used with chdir)
#include <fcntl.h>     // For open(), O_WRONLY, O_CREAT, O_TRUNC

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

/**
 * Parse command line and extract stdout redirect file if present.
 * Returns the index where redirection starts, or -1 if no redirection.
 */
int find_stdout_redirect(char **args, char **redirect_file)
{
  *redirect_file = NULL;

  for (int i = 0; args[i] != NULL; i++)
  {
    // Check for > or 1>
    if (strcmp(args[i], ">") == 0 || strcmp(args[i], "1>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for stdout redirection\n");
        return -2; // Error case
      }
      *redirect_file = args[i + 1];
      return i; // Return index of redirect operator
    }
  }

  return -1; // No redirection found
}

/**
 * Parse command line and extract stderr redirect file if present.
 * Returns the index where redirection starts, or -1 if no redirection.
 */
int find_stderr_redirect(char **args, char **redirect_file)
{
  *redirect_file = NULL;

  for (int i = 0; args[i] != NULL; i++)
  {
    // Check for 2>
    if (strcmp(args[i], "2>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for stderr redirection\n");
        return -2; // Error case
      }
      *redirect_file = args[i + 1];
      return i; // Return index of redirect operator
    }
  }

  return -1; // No redirection found
}

/**
 * Execute echo builtin with optional output redirection.
 */
void execute_echo(char **args, int redirect_idx, char *redirect_file)
{
  FILE *output = stdout;

  // Handle stdout redirection
  if (redirect_file != NULL)
  {
    output = fopen(redirect_file, "w");
    if (output == NULL)
    {
      perror("fopen");
      return;
    }
  }

  // Print arguments (up to redirect operator if present)
  int end_idx = (redirect_idx >= 0) ? redirect_idx : MAX_ARGS;
  for (int i = 1; args[i] != NULL && i < end_idx; i++)
  {
    if (i > 1)
      fprintf(output, " ");
    fprintf(output, "%s", args[i]);
  }
  fprintf(output, "\n");

  if (output != stdout)
    fclose(output);
}

/**
 * Execute pwd builtin with optional output redirection.
 */
void execute_pwd(char *redirect_file)
{
  char cwd_buffer[MAX_PATH_LENGTH];
  FILE *output = stdout;

  if (redirect_file != NULL)
  {
    output = fopen(redirect_file, "w");
    if (output == NULL)
    {
      perror("fopen");
      return;
    }
  }

  if (getcwd(cwd_buffer, sizeof(cwd_buffer)) != NULL)
  {
    fprintf(output, "%s\n", cwd_buffer);
  }
  else
  {
    perror("getcwd");
  }

  if (output != stdout)
    fclose(output);
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

    // --- Parse command with quote handling ---
    char *args[MAX_ARGS];
    int arg_index = 0;

    char *read_ptr = command;
    char *write_ptr = command;

    int state = STATE_DEFAULT;
    args[arg_index] = write_ptr;
    int new_arg = 1;

    while (*read_ptr != '\0')
    {
      char c = *read_ptr;

      if (state == STATE_DEFAULT)
      {
        if (c == '\\')
        {
          read_ptr++;
          if (*read_ptr == '\0')
            break;
          *write_ptr = *read_ptr;
          write_ptr++;
          read_ptr++;
          new_arg = 0;
        }
        else if (c == ' ')
        {
          if (!new_arg)
          {
            *write_ptr = '\0';
            write_ptr++;

            arg_index++;
            if (arg_index >= MAX_ARGS - 1)
            {
              fprintf(stderr, "Error: Too many arguments\n");
              break;
            }
            args[arg_index] = write_ptr;
            new_arg = 1;
          }
          read_ptr++;
        }
        else if (c == '\'')
        {
          state = STATE_IN_QUOTE;
          read_ptr++;
          new_arg = 0;
        }
        else if (c == '"')
        {
          state = STATE_IN_DQUOTE;
          read_ptr++;
          new_arg = 0;
        }
        else
        {
          *write_ptr = c;
          write_ptr++;
          read_ptr++;
          new_arg = 0;
        }
      }
      else if (state == STATE_IN_QUOTE)
      {
        if (c == '\'')
        {
          state = STATE_DEFAULT;
          read_ptr++;
        }
        else
        {
          *write_ptr = c;
          write_ptr++;
          read_ptr++;
        }
      }
      else if (state == STATE_IN_DQUOTE)
      {
        if (c == '\\')
        {
          if (read_ptr[1] == '\\' || read_ptr[1] == '"')
          {
            read_ptr++;
            *write_ptr = *read_ptr;
            write_ptr++;
            read_ptr++;
          }
          else
          {
            *write_ptr = c;
            write_ptr++;
            read_ptr++;
          }
        }
        else if (c == '"')
        {
          state = STATE_DEFAULT;
          read_ptr++;
        }
        else
        {
          *write_ptr = c;
          write_ptr++;
          read_ptr++;
        }
      }
    }

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

    *write_ptr = '\0';

    if (!new_arg)
    {
      arg_index++;
    }
    args[arg_index] = NULL;

    if (args[0] == NULL)
    {
      continue;
    }

    // --- Check for output redirection ---
    char *stdout_redirect_file = NULL;
    int stdout_redirect_idx = find_stdout_redirect(args, &stdout_redirect_file);

    if (stdout_redirect_idx == -2)
    {
      continue; // Error in stdout redirection syntax
    }

    // --- Check for stderr redirection ---
    char *stderr_redirect_file = NULL;
    int stderr_redirect_idx = find_stderr_redirect(args, &stderr_redirect_file);

    if (stderr_redirect_idx == -2)
    {
      continue; // Error in stderr redirection syntax
    }

    // Find the first redirection operator to truncate args for builtins
    int redirect_idx = -1;
    if (stdout_redirect_idx >= 0 && stderr_redirect_idx >= 0)
    {
      redirect_idx = (stdout_redirect_idx < stderr_redirect_idx) ? stdout_redirect_idx : stderr_redirect_idx;
    }
    else if (stdout_redirect_idx >= 0)
    {
      redirect_idx = stdout_redirect_idx;
    }
    else if (stderr_redirect_idx >= 0)
    {
      redirect_idx = stderr_redirect_idx;
    }

    char *temp_arg = NULL;
    int temp_idx = -1;

    // Temporarily null-terminate at the *first* redirect for builtins
    if (redirect_idx >= 0)
    {
      temp_idx = redirect_idx;
      temp_arg = args[temp_idx];
      args[temp_idx] = NULL;
    }

    // --- Builtin Command Handling ---

    // 5. Check for 'exit 0'
    if (strcmp(args[0], "exit") == 0)
    {
      return 0;
    }

    // 6. Check for 'pwd'
    if (strcmp(args[0], "pwd") == 0)
    {
      execute_pwd(stdout_redirect_file);
      if (temp_idx >= 0)
        args[temp_idx] = temp_arg; // Restore arg
      continue;
    }

    // 7. Check for 'echo'
    if (strcmp(args[0], "echo") == 0)
    {
      // echo builtin only cares about stdout redirection
      execute_echo(args, stdout_redirect_idx, stdout_redirect_file);
      if (temp_idx >= 0)
        args[temp_idx] = temp_arg; // Restore arg
      continue;
    }

    // 8. Check for 'cd'
    if (strcmp(args[0], "cd") == 0)
    {
      if (args[1] == NULL)
      {
        fprintf(stderr, "cd: missing argument\n");
        if (temp_idx >= 0)
          args[temp_idx] = temp_arg; // Restore arg
        continue;
      }

      char *path_to_change = NULL;

      if (strcmp(args[1], "~") == 0)
      {
        path_to_change = getenv("HOME");
        if (path_to_change == NULL)
        {
          fprintf(stderr, "cd: HOME not set\n");
          if (temp_idx >= 0)
            args[temp_idx] = temp_arg; // Restore arg
          continue;
        }
      }
      else
      {
        path_to_change = args[1];
      }

      if (chdir(path_to_change) != 0)
      {
        fprintf(stderr, "cd: %s: %s\n", args[1], strerror(errno));
      }
      if (temp_idx >= 0)
        args[temp_idx] = temp_arg; // Restore arg
      continue;
    }

    // 9. Check for 'type'
    if (strcmp(args[0], "type") == 0)
    {
      if (args[1] == NULL)
      {
        fprintf(stderr, "type: missing argument\n");
        if (temp_idx >= 0)
          args[temp_idx] = temp_arg; // Restore arg
        continue;
      }

      char *arg = args[1];

      // TODO: 'type' builtin should also respect stdout redirection
      // For now, it prints directly to stdout
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
      if (temp_idx >= 0)
        args[temp_idx] = temp_arg; // Restore arg
      continue;
    }

    // Restore args if we modified it
    if (temp_idx >= 0)
    {
      args[temp_idx] = temp_arg;
    }

    // --- External Command Execution ---

    // Null-terminate args at *all* redirect locations for execv
    if (stdout_redirect_idx >= 0)
    {
      args[stdout_redirect_idx] = NULL;
    }
    if (stderr_redirect_idx >= 0)
    {
      args[stderr_redirect_idx] = NULL;
    }

    char *cmd_name = args[0];
    char full_path[MAX_PATH_LENGTH];

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

        // Handle output (stdout) redirection
        if (stdout_redirect_file != NULL)
        {
          int fd = open(stdout_redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1)
          {
            perror("open (stdout)");
            exit(EXIT_FAILURE);
          }

          // Redirect stdout to the file
          if (dup2(fd, STDOUT_FILENO) == -1)
          {
            perror("dup2 (stdout)");
            close(fd);
            exit(EXIT_FAILURE);
          }
          close(fd);
        }

        // Handle error (stderr) redirection
        if (stderr_redirect_file != NULL)
        {
          int fd = open(stderr_redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1)
          {
            perror("open (stderr)");
            exit(EXIT_FAILURE);
          }

          // Redirect stderr to the file
          if (dup2(fd, STDERR_FILENO) == -1)
          {
            perror("dup2 (stderr)");
            close(fd);
            exit(EXIT_FAILURE);
          }
          close(fd);
        }

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
      fprintf(stderr, "%s: command not found\n", cmd_name);
    }
  }

  return 0;
}
