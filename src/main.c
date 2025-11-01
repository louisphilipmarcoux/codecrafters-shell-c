#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // For strcmp, strncmp, strlen, strdup, strtok, strerror
#include <unistd.h>    // For access(), X_OK, fork(), execv(), getcwd(), chdir(), pipe(), dup2(), close()
#include <sys/wait.h>  // For waitpid()
#include <sys/types.h> // For pid_t
#include <errno.h>     // For errno (used with chdir)
#include <fcntl.h>     // For open(), O_WRONLY, O_CREAT, O_TRUNC, O_APPEND
#include <dirent.h>    // For opendir(), readdir(), closedir()
#include <readline/readline.h>
#include <readline/history.h>

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
 * Readline completion generator for builtin commands.
 * This function is called repeatedly to generate matches.
 */
char *builtin_generator(const char *text, int state)
{
  static const char *builtins[] = {
      "echo", "exit", "type", "pwd", "cd", NULL};
  static int list_index, len;

  // Initialize on first call (state == 0)
  if (!state)
  {
    list_index = 0;
    len = strlen(text);
  }

  // Find next matching builtin
  while (builtins[list_index])
  {
    const char *name = builtins[list_index];
    list_index++;

    if (strncmp(name, text, len) == 0)
    {
      return strdup(name);
    }
  }

  return NULL;
}

/**
 * Readline completion generator for executables in PATH.
 * This function is called repeatedly to generate matches.
 */
char *path_generator(const char *text, int state)
{
  static char *path_copy = NULL;
  static char *current_dir = NULL;
  static DIR *dir_handle = NULL;
  static int text_len;

  // Initialize on first call (state == 0)
  if (!state)
  {
    text_len = strlen(text);

    // Clean up any previous state
    if (dir_handle)
    {
      closedir(dir_handle);
      dir_handle = NULL;
    }
    if (path_copy)
    {
      free(path_copy);
      path_copy = NULL;
    }

    // Get PATH environment variable
    char *path_env = getenv("PATH");
    if (path_env == NULL)
      return NULL;

    path_copy = strdup(path_env);
    if (path_copy == NULL)
      return NULL;

    current_dir = strtok(path_copy, ":");
  }

  // Search through directories in PATH
  while (current_dir != NULL)
  {
    // Open directory if not already open
    if (dir_handle == NULL)
    {
      dir_handle = opendir(current_dir);
      if (dir_handle == NULL)
      {
        // Directory doesn't exist or can't be opened, move to next
        current_dir = strtok(NULL, ":");
        continue;
      }
    }

    // Read entries from current directory
    struct dirent *entry;
    while ((entry = readdir(dir_handle)) != NULL)
    {
      // Skip . and ..
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      // Check if entry matches the text
      if (strncmp(entry->d_name, text, text_len) == 0)
      {
        // Check if it's executable
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, entry->d_name);

        if (access(full_path, X_OK) == 0)
        {
          return strdup(entry->d_name);
        }
      }
    }

    // Close current directory and move to next
    closedir(dir_handle);
    dir_handle = NULL;
    current_dir = strtok(NULL, ":");
  }

  // Cleanup
  if (dir_handle)
  {
    closedir(dir_handle);
    dir_handle = NULL;
  }
  if (path_copy)
  {
    free(path_copy);
    path_copy = NULL;
  }

  return NULL;
}

/**
 * Combined generator that tries builtins first, then PATH executables.
 */
char *command_generator(const char *text, int state)
{
  static int checking_builtins;
  char *result;

  // On first call, start with builtins
  if (!state)
  {
    checking_builtins = 1;
  }

  // Try builtins first
  if (checking_builtins)
  {
    result = builtin_generator(text, state);
    if (result != NULL)
      return result;

    // Done with builtins, move to PATH executables
    checking_builtins = 0;
    state = 0; // Reset state for path_generator
  }

  // Try PATH executables
  return path_generator(text, state);
}

/**
 * Readline completion function.
 * Attempts to complete builtin commands and PATH executables.
 */
char **shell_completion(const char *text, int start, int end)
{
  char **matches = NULL;

  // Only complete if we're at the start of the line (completing command name)
  if (start == 0)
  {
    matches = rl_completion_matches(text, command_generator);
  }

  // Return matches or NULL - this prevents default filename completion
  return matches;
}

/**
 * Initialize readline with custom completion.
 */
void init_readline(void)
{
  // Set custom completion function
  rl_attempted_completion_function = shell_completion;

  // Append a space after successful completion
  rl_completion_append_character = ' ';
}

/**
 * Parse command line and extract redirect files if present.
 * Returns the index where redirection starts, or -1 if no redirection.
 * Updates redirect_stdout, redirect_stderr with filenames.
 * Updates append_stdout and append_stderr to indicate if >> is used instead of >.
 */
int find_redirect(char **args, char **redirect_stdout, char **redirect_stderr, int *append_stdout, int *append_stderr)
{
  *redirect_stdout = NULL;
  *redirect_stderr = NULL;
  *append_stdout = 0;
  *append_stderr = 0;
  int first_redirect_idx = -1;

  for (int i = 0; args[i] != NULL; i++)
  {
    // Check for >> or 1>> (stdout append)
    if (strcmp(args[i], ">>") == 0 || strcmp(args[i], "1>>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; // Error case
      }
      *redirect_stdout = args[i + 1];
      *append_stdout = 1;
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    }
    // Check for > or 1> (stdout)
    else if (strcmp(args[i], ">") == 0 || strcmp(args[i], "1>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; // Error case
      }
      *redirect_stdout = args[i + 1];
      *append_stdout = 0;
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    }
    // Check for 2>> (stderr append)
    else if (strcmp(args[i], "2>>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; // Error case
      }
      *redirect_stderr = args[i + 1];
      *append_stderr = 1;
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    }
    // Check for 2> (stderr)
    else if (strcmp(args[i], "2>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; // Error case
      }
      *redirect_stderr = args[i + 1];
      *append_stderr = 0;
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    }
  }

  return first_redirect_idx;
}

/**
 * Execute echo builtin with optional output redirection.
 */
void execute_echo(char **args, int redirect_idx, char *redirect_stdout, char *redirect_stderr, int append_stdout, int append_stderr)
{
  FILE *output = stdout;
  FILE *error = stderr;

  // Handle stdout redirection
  if (redirect_stdout != NULL)
  {
    const char *mode = append_stdout ? "a" : "w";
    output = fopen(redirect_stdout, mode);
    if (output == NULL)
    {
      perror("fopen");
      return;
    }
  }

  // Handle stderr redirection (echo doesn't produce stderr, but for consistency)
  if (redirect_stderr != NULL)
  {
    const char *mode = append_stderr ? "a" : "w";
    error = fopen(redirect_stderr, mode);
    if (error == NULL)
    {
      perror("fopen");
      if (output != stdout)
        fclose(output);
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
  if (error != stderr)
    fclose(error);
}

/**
 * Execute pwd builtin with optional output redirection.
 */
void execute_pwd(char *redirect_stdout, char *redirect_stderr, int append_stdout, int append_stderr)
{
  char cwd_buffer[MAX_PATH_LENGTH];
  FILE *output = stdout;
  FILE *error = stderr;

  if (redirect_stdout != NULL)
  {
    const char *mode = append_stdout ? "a" : "w";
    output = fopen(redirect_stdout, mode);
    if (output == NULL)
    {
      perror("fopen");
      return;
    }
  }

  if (redirect_stderr != NULL)
  {
    const char *mode = append_stderr ? "a" : "w";
    error = fopen(redirect_stderr, mode);
    if (error == NULL)
    {
      perror("fopen");
      if (output != stdout)
        fclose(output);
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
  if (error != stderr)
    fclose(error);
}

int main(int argc, char *argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL);

  // Initialize readline
  init_readline();

  while (1)
  {
    // 1. Read the user's input using readline
    char *input = readline("$ ");

    // Check for EOF (Ctrl+D)
    if (input == NULL)
    {
      printf("\n");
      break;
    }

    // 2. Handle empty input
    if (strlen(input) == 0)
    {
      free(input);
      continue;
    }

    // Add to history
    add_history(input);

    // Copy input to command buffer for processing
    char command[MAX_COMMAND_LENGTH];
    strncpy(command, input, MAX_COMMAND_LENGTH - 1);
    command[MAX_COMMAND_LENGTH - 1] = '\0';

    // Free the readline buffer
    free(input);

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
        else if (c == '|')
        {
          // Pipe operator - treat as separate token
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
          }
          // Add pipe as its own argument
          args[arg_index] = write_ptr;
          *write_ptr = '|';
          write_ptr++;
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
          read_ptr++;
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

    *write_ptr = '\0'; // Terminate the last token

    if (new_arg)
    {
      // We ended on a separator (space or pipe)
      if (arg_index == 0 && *args[0] == '\0')
      {
        // Special case: input was just whitespace
        args[0] = NULL;
      }
      else
      {
        // We ended with "cat | " or "cat file |"
        // The last arg pointer (e.g., args[3]) points to an empty string.
        // We must set this to NULL.
        args[arg_index] = NULL;
      }
    }
    else
    {
      // We ended in the middle of an argument (e.g., "wc")
      // This argument is valid, so we increment arg_index
      // and set the *next* pointer to NULL.
      arg_index++;
      args[arg_index] = NULL;
    }

    if (args[0] == NULL)
    {
      continue;
    }

    // --- Check for pipeline ---
    char **args_cmd1 = args;
    char **args_cmd2 = NULL;
    int pipe_idx = -1;
    pid_t pid1 = -1, pid2 = -1;

    for (int i = 0; args[i] != NULL; i++)
    {
      if (strcmp(args[i], "|") == 0)
      {
        if (i == 0 || args[i + 1] == NULL)
        {
          fprintf(stderr, "Error: Invalid pipe syntax\n");
          pipe_idx = -2; // Mark as error
          break;
        }
        pipe_idx = i;
        args[i] = NULL;           // Terminate cmd1
        args_cmd2 = &args[i + 1]; // Set cmd2 to start after "|"
        break;                    // Only handle one pipe (two commands)
      }
    }

    if (pipe_idx == -2)
    {
      continue; // Invalid pipe syntax
    }

    // --- Check for output redirection ---
    // This applies to the *last* command in the sequence
    char *redirect_stdout = NULL;
    char *redirect_stderr = NULL;
    int append_stdout = 0;
    int append_stderr = 0;
    int redirect_idx = -1;

    // Determine which command to check for redirection
    char **cmd_to_redirect = (pipe_idx != -1) ? args_cmd2 : args_cmd1;

    redirect_idx = find_redirect(cmd_to_redirect, &redirect_stdout, &redirect_stderr, &append_stdout, &append_stderr);

    if (redirect_idx == -2)
    {
      continue; // Error in redirection syntax
    }

    // Null-terminate args at first redirect operator
    if (redirect_idx >= 0)
    {
      cmd_to_redirect[redirect_idx] = NULL;
    }

    // --- Execution ---
    if (pipe_idx != -1)
    {
      // --- Pipeline Execution (cmd1 | cmd2) ---
      //

      int pipefd[2];
      if (pipe(pipefd) == -1)
      {
        perror("pipe");
        continue;
      }

      // Fork for Command 1
      pid1 = fork();
      if (pid1 == -1)
      {
        perror("fork (pid1)");
        close(pipefd[0]);
        close(pipefd[1]);
        continue;
      }

      if (pid1 == 0)
      {
        // --- Child 1 (cmd1) ---
        close(pipefd[0]); // Close unused read end

        // Redirect stdout to the pipe's write end
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
          perror("dup2 (cmd1)");
          exit(EXIT_FAILURE);
        }
        close(pipefd[1]); // Close original write end

        // Find and exec cmd1
        char full_path_cmd1[MAX_PATH_LENGTH];
        if (find_executable(args_cmd1[0], full_path_cmd1, MAX_PATH_LENGTH))
        {
          if (execv(full_path_cmd1, args_cmd1) == -1)
          {
            perror("execv (cmd1)");
            exit(EXIT_FAILURE);
          }
        }
        else
        {
          fprintf(stderr, "%s: command not found\n", args_cmd1[0]);
          exit(EXIT_FAILURE);
        }
      }

      // Fork for Command 2
      pid2 = fork();
      if (pid2 == -1)
      {
        perror("fork (pid2)");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0); // Clean up first child
        continue;
      }

      if (pid2 == 0)
      {
        // --- Child 2 (cmd2) ---
        close(pipefd[1]); // Close unused write end

        // Redirect stdin from the pipe's read end
        if (dup2(pipefd[0], STDIN_FILENO) == -1)
        {
          perror("dup2 (cmd2)");
          exit(EXIT_FAILURE);
        }
        close(pipefd[0]); // Close original read end

        // Handle output redirection for cmd2
        if (redirect_stdout != NULL)
        {
          int flags = O_WRONLY | O_CREAT;
          flags |= append_stdout ? O_APPEND : O_TRUNC;
          int fd = open(redirect_stdout, flags, 0644);
          if (fd == -1)
          {
            perror("open (stdout)");
            exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDOUT_FILENO) == -1)
          {
            perror("dup2 (stdout)");
            close(fd);
            exit(EXIT_FAILURE);
          }
          close(fd);
        }
        if (redirect_stderr != NULL)
        {
          int flags = O_WRONLY | O_CREAT;
          flags |= append_stderr ? O_APPEND : O_TRUNC;
          int fd = open(redirect_stderr, flags, 0644);
          if (fd == -1)
          {
            perror("open (stderr)");
            exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDERR_FILENO) == -1)
          {
            perror("dup2 (stderr)");
            close(fd);
            exit(EXIT_FAILURE);
          }
          close(fd);
        }

        // Find and exec cmd2
        char full_path_cmd2[MAX_PATH_LENGTH];
        if (find_executable(args_cmd2[0], full_path_cmd2, MAX_PATH_LENGTH))
        {
          if (execv(full_path_cmd2, args_cmd2) == -1)
          {
            perror("execv (cmd2)");
            exit(EXIT_FAILURE);
          }
        }
        else
        {
          fprintf(stderr, "%s: command not found\n", args_cmd2[0]);
          exit(EXIT_FAILURE);
        }
      }

      // --- Parent (Shell) ---
      // Close both ends of the pipe in the parent
      // This is crucial for EOF to be sent to cmd2 when cmd1 finishes
      close(pipefd[0]);
      close(pipefd[1]);

      // Wait for both children to complete
      waitpid(pid1, NULL, 0);
      waitpid(pid2, NULL, 0);
    }
    else
    {
      // --- Single Command Execution (No Pipe) ---

      // --- Builtin Command Handling ---

      // 5. Check for 'exit 0'
      if (strcmp(args_cmd1[0], "exit") == 0)
      {
        return 0;
      }

      // 6. Check for 'pwd'
      if (strcmp(args_cmd1[0], "pwd") == 0)
      {
        execute_pwd(redirect_stdout, redirect_stderr, append_stdout, append_stderr);
        continue;
      }

      // 7. Check for 'echo'
      if (strcmp(args_cmd1[0], "echo") == 0)
      {
        execute_echo(args_cmd1, redirect_idx, redirect_stdout, redirect_stderr, append_stdout, append_stderr);
        continue;
      }

      // 8. Check for 'cd'
      if (strcmp(args_cmd1[0], "cd") == 0)
      {
        if (args_cmd1[1] == NULL)
        {
          fprintf(stderr, "cd: missing argument\n");
          continue;
        }

        char *path_to_change = NULL;

        if (strcmp(args_cmd1[1], "~") == 0)
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
          path_to_change = args_cmd1[1];
        }

        if (chdir(path_to_change) != 0)
        {
          fprintf(stderr, "cd: %s: %s\n", args_cmd1[1], strerror(errno));
        }
        continue;
      }

      // 9. Check for 'type'
      if (strcmp(args_cmd1[0], "type") == 0)
      {
        if (args_cmd1[1] == NULL)
        {
          fprintf(stderr, "type: missing argument\n");
          continue;
        }

        char *arg = args_cmd1[1];

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

      // --- External Command Execution ---
      char *cmd_name = args_cmd1[0];
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

          // Handle stdout redirection
          if (redirect_stdout != NULL)
          {
            int flags = O_WRONLY | O_CREAT;
            flags |= append_stdout ? O_APPEND : O_TRUNC;

            int fd = open(redirect_stdout, flags, 0644);
            if (fd == -1)
            {
              perror("open");
              exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDOUT_FILENO) == -1)
            {
              perror("dup2");
              close(fd);
              exit(EXIT_FAILURE);
            }

            close(fd);
          }

          // Handle stderr redirection
          if (redirect_stderr != NULL)
          {
            int flags = O_WRONLY | O_CREAT;
            flags |= append_stderr ? O_APPEND : O_TRUNC;

            int fd = open(redirect_stderr, flags, 0644);
            if (fd == -1)
            {
              perror("open");
              exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDERR_FILENO) == -1)
            {
              perror("dup2");
              close(fd);
              exit(EXIT_FAILURE);
            }

            close(fd);
          }

          if (execv(full_path, args_cmd1) == -1)
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
  }

  return 0;
}