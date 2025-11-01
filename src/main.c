#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // For strcmp, strncmp, strlen, strdup, strtok, strerror
#include <unistd.h>    // For access(), X_OK, fork(), execv(), getcwd(), chdir(), pipe()
#include <sys/wait.h>  // For waitpid()
#include <sys/types.h> // For pid_t
#include <errno.h>     // For errno (used with chdir)
#include <fcntl.h>     // For open(), O_WRONLY, O_CREAT, O_TRUNC
#include <dirent.h>    // For opendir(), readdir(), closedir()
[cite_start]           /* Prefer readline; [cite: 1]
              try editline as an alternative; if neither is available,
              provide minimal prototypes so the source compiles (these are only used
              [cite_start]when the real library is missing and do not implement functionality). [cite: 2]
            */
#if defined(__has_include)
#if __has_include(<readline/readline.h>)
#include <readline/readline.h>
#include <readline/history.h>
#elif __has_include(<editline/readline.h>)
#include <editline/readline.h>
    [cite_start] #/* editline may not have a separate history header; [cite: 3]
        provide add_history if missing */
#if __has_include(<readline/history.h>)
#include <readline/history.h>
#endif
#else
    /* Minimal readline/editline stubs to avoid compilation errors when headers
       [cite_start]are not present; [cite: 4]
       these do not provide real functionality. */
    char *readline(const char *prompt);
void add_history(const char *line);
/* rl_attempted_completion_function points to a function with signature
   char **func(const char *text, int start, int end) */
[cite_start] extern char **(*rl_attempted_completion_function)(const char *, int, int);
[cite:6]
    [cite_start] extern int rl_completion_append_character;
[cite:7]
    /* rl_completion_matches has a generator function type of
       char *gen(const char *text, int state) */
    char **
    rl_completion_matches(const char *text, char *(*generator)(const char *, int));
#endif
#else
#include <readline/readline.h>
#include <readline/history.h>
[cite_start]#endif [cite: 8]

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
 * [cite_start]Helper function to find an executable in PATH. [cite: 9]
 */
[cite_start]int find_executable(const char *command, char *full_path, size_t full_path_size) [cite: 10]
{
  char *path_env = getenv("PATH");
  [cite_start]if (path_env == NULL) [cite: 11]
    return 0;

  char *path_copy = strdup(path_env);
  [cite_start]if (path_copy == NULL) [cite: 12]
  {
    perror("strdup");
    return 0;
  }

  char *dir = strtok(path_copy, ":");
  [cite_start]while (dir != NULL) [cite: 13]
  {
    snprintf(full_path, full_path_size, "%s/%s", dir, command);
    [cite_start]if (access(full_path, X_OK) == 0) [cite: 14]
    {
      free(path_copy);
      return 1;
      [cite_start]// Found [cite: 15]
    }
    dir = strtok(NULL, ":");
  }

  free(path_copy);
  return 0;
  [cite_start]// Not found [cite: 16]
}

/**
 * Readline completion generator for builtin commands.
 * [cite_start]This function is called repeatedly to generate matches. [cite: 17]
 */
char *builtin_generator(const char *text, int state)
{
  static const char *builtins[] = {
      "echo", "exit", "type", "pwd", "cd", NULL};
  [cite_start]static int list_index, len; [cite: 18]

  // Initialize on first call (state == 0)
  if (!state)
  {
    list_index = 0;
    [cite_start]len = strlen(text); [cite: 19]
  }

  // Find next matching builtin
  while (builtins[list_index])
  {
    const char *name = builtins[list_index];
    [cite_start]list_index++; [cite: 20]

    if (strncmp(name, text, len) == 0)
    {
      return strdup(name);
    [cite_start]} [cite: 21]
  }

  return NULL;
}

/**
 * Readline completion generator for executables in PATH.
 * [cite_start]This function is called repeatedly to generate matches. [cite: 22]
 */
char *path_generator(const char *text, int state)
{
  static char *path_copy = NULL;
  [cite_start]static char *current_dir = NULL; [cite: 23]
  static DIR *dir_handle = NULL;
  static int text_len;
  [cite_start]// Initialize on first call (state == 0) [cite: 24]
  if (!state)
  {
    text_len = strlen(text);
    [cite_start]// Clean up any previous state [cite: 25]
    if (dir_handle)
    {
      closedir(dir_handle);
      [cite_start]dir_handle = NULL; [cite: 26]
    }
    if (path_copy)
    {
      free(path_copy);
      [cite_start]path_copy = NULL; [cite: 27]
    }

    // Get PATH environment variable
    char *path_env = getenv("PATH");
    [cite_start]if (path_env == NULL) [cite: 28]
      return NULL;

    path_copy = strdup(path_env);
    [cite_start]if (path_copy == NULL) [cite: 29]
      return NULL;

    current_dir = strtok(path_copy, ":");
  [cite_start]} [cite: 30]

  // Search through directories in PATH
  while (current_dir != NULL)
  {
    // Open directory if not already open
    if (dir_handle == NULL)
    {
      dir_handle = opendir(current_dir);
      [cite_start]if (dir_handle == NULL) [cite: 31]
      {
        // Directory doesn't exist or can't be opened, move to next
        current_dir = strtok(NULL, ":");
        [cite_start]continue; [cite: 32]
      }
    }

    // Read entries from current directory
    struct dirent *entry;
    [cite_start]while ((entry = readdir(dir_handle)) != NULL) [cite: 33]
    {
      [cite_start]// Skip . [cite: 34]
      // and ..
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      [cite_start]// Check if entry matches the text [cite: 35]
      if (strncmp(entry->d_name, text, text_len) == 0)
      {
        // Check if it's executable
        char full_path[MAX_PATH_LENGTH];
        [cite_start]snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, entry->d_name); [cite: 36]

        if (access(full_path, X_OK) == 0)
        {
          return strdup(entry->d_name);
        [cite_start]} [cite: 37]
      }
    }

    // Close current directory and move to next
    closedir(dir_handle);
    [cite_start]dir_handle = NULL; [cite: 38]
    current_dir = strtok(NULL, ":");
  }

  // Cleanup
  if (dir_handle)
  {
    closedir(dir_handle);
    [cite_start]dir_handle = NULL; [cite: 39]
  }
  if (path_copy)
  {
    free(path_copy);
    path_copy = NULL;
  }

  return NULL;
[cite_start]} [cite: 40]

/**
 * [cite_start]Combined generator that tries builtins first, then PATH executables. [cite: 41]
 */
char *command_generator(const char *text, int state)
{
  static int checking_builtins;
  char *result;
  [cite_start]// On first call, start with builtins [cite: 42]
  if (!state)
  {
    checking_builtins = 1;
  [cite_start]} [cite: 43]

  // Try builtins first
  if (checking_builtins)
  {
    result = builtin_generator(text, state);
    [cite_start]if (result != NULL) [cite: 44]
      return result;
    [cite_start]// Done with builtins, move to PATH executables [cite: 45]
    checking_builtins = 0;
    state = 0;
    [cite_start]// Reset state for path_generator [cite: 46]
  }

  // Try PATH executables
  return path_generator(text, state);
[cite_start]} [cite: 47]

/**
 * Readline completion function.
 * [cite_start]Attempts to complete builtin commands and PATH executables. [cite: 48]
 */
char **shell_completion(const char *text, int start, int end)
{
  char **matches = NULL;
  [cite_start]// Only complete if we're at the start of the line (completing command name) [cite: 49]
  if (start == 0)
  {
    matches = rl_completion_matches(text, command_generator);
  [cite_start]} [cite: 50]

  // Return matches or NULL - this prevents default filename completion
  return matches;
[cite_start]} [cite: 51]

/**
 * Initialize readline with custom completion.
 */
void init_readline(void)
{
  // Set custom completion function
  rl_attempted_completion_function = shell_completion;
  [cite_start]// Append a space after successful completion [cite: 52]
  rl_completion_append_character = ' ';
[cite_start]} [cite: 53]

/**
 * Find the position of the pipe operator in args.
 * [cite_start]Returns the index of "|", or -1 if not found. [cite: 54]
 [cite_start]*/ [cite: 55]
int find_pipe(char **args)
{
  for (int i = 0; args[i] != NULL; i++)
  {
    if (strcmp(args[i], "|") == 0)
    {
      return i;
    [cite_start]} [cite: 56]
  }
  return -1;
}

/**
 * Parse command line and extract redirect files if present.
 * [cite_start]Returns the index where redirection starts, or -1 if no redirection. [cite: 57]
 * Updates redirect_stdout, redirect_stderr with filenames.
 * [cite_start]Updates append_stdout and append_stderr to indicate if >> is used instead of >. [cite: 58]
 [cite_start]*/ [cite: 59]
int find_redirect(char **args, char **redirect_stdout, char **redirect_stderr, int *append_stdout, int *append_stderr)
{
  *redirect_stdout = NULL;
  *redirect_stderr = NULL;
  [cite_start]*append_stdout = 0; [cite: 60]
  *append_stderr = 0;
  int first_redirect_idx = -1;
  [cite_start]for (int i = 0; args[i] != NULL; i++) [cite: 61]
  {
    // Check for >> or 1>> (stdout append)
    if (strcmp(args[i], ">>") == 0 || strcmp(args[i], "1>>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; [cite_start]// Error case [cite: 62]
      }
      *redirect_stdout = args[i + 1];
      [cite_start]*append_stdout = 1; [cite: 63]
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    [cite_start]} [cite: 64]
    // Check for > or 1> (stdout)
    else if (strcmp(args[i], ">") == 0 || strcmp(args[i], "1>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; [cite_start]// Error case [cite: 65]
      }
      *redirect_stdout = args[i + 1];
      [cite_start]*append_stdout = 0; [cite: 66]
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    [cite_start]} [cite: 67]
    // Check for 2>> (stderr append)
    else if (strcmp(args[i], "2>>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; [cite_start]// Error case [cite: 68]
      }
      *redirect_stderr = args[i + 1];
      [cite_start]*append_stderr = 1; [cite: 69]
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    [cite_start]} [cite: 70]
    // Check for 2> (stderr)
    else if (strcmp(args[i], "2>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "Error: No file specified for redirection\n");
        return -2; [cite_start]// Error case [cite: 71]
      }
      *redirect_stderr = args[i + 1];
      [cite_start]*append_stderr = 0; [cite: 72]
      if (first_redirect_idx == -1)
        first_redirect_idx = i;
    [cite_start]} [cite: 73]
  }

  return first_redirect_idx;
}

/**
 * [cite_start]Execute echo builtin with optional output redirection. [cite: 74]
 */
void execute_echo(char **args, int redirect_idx, char *redirect_stdout, char *redirect_stderr, int append_stdout, int append_stderr)
{
  FILE *output = stdout;
  [cite_start]FILE *error = stderr; [cite: 75]

  // Handle stdout redirection
  if (redirect_stdout != NULL)
  {
    [cite_start]const char *mode = append_stdout ? [cite: 76]
                                    "a" : "w";
    output = fopen(redirect_stdout, mode);
    if (output == NULL)
    {
      perror("fopen");
      [cite_start]return; [cite: 77]
    }
  }

  // Handle stderr redirection (echo doesn't produce stderr, but for consistency)
  if (redirect_stderr != NULL)
  {
    [cite_start]const char *mode = append_stderr ? [cite: 78]
                                    "a" : "w";
    error = fopen(redirect_stderr, mode);
    if (error == NULL)
    {
      perror("fopen");
      [cite_start]if (output != stdout) [cite: 79]
        fclose(output);
      return;
    [cite_start]} [cite: 80]
  }

  // Print arguments (up to redirect operator if present)
  [cite_start]int end_idx = (redirect_idx >= 0) ? [cite: 81]
                                    redirect_idx : MAX_ARGS;
  for (int i = 1; args[i] != NULL && i < end_idx; i++)
  {
    if (i > 1)
      fprintf(output, " ");
    [cite_start]fprintf(output, "%s", args[i]); [cite: 82]
  }
  fprintf(output, "\n");

  if (output != stdout)
    fclose(output);
  [cite_start]if (error != stderr) [cite: 83]
    fclose(error);
}

/**
 * [cite_start]Execute pwd builtin with optional output redirection. [cite: 84]
 */
void execute_pwd(char *redirect_stdout, char *redirect_stderr, int append_stdout, int append_stderr)
{
  char cwd_buffer[MAX_PATH_LENGTH];
  FILE *output = stdout;
  FILE *error = stderr;
  [cite_start]if (redirect_stdout != NULL) [cite: 85]
  {
    const char *mode = append_stdout ? "a" : "w";
    [cite_start]output = fopen(redirect_stdout, mode); [cite: 86]
    if (output == NULL)
    {
      perror("fopen");
      return;
    [cite_start]} [cite: 87]
  }

  if (redirect_stderr != NULL)
  {
    [cite_start]const char *mode = append_stderr ? [cite: 88]
                                    "a" : "w";
    error = fopen(redirect_stderr, mode);
    if (error == NULL)
    {
      perror("fopen");
      [cite_start]if (output != stdout) [cite: 89]
        fclose(output);
      return;
    [cite_start]} [cite: 90]
  }

  if (getcwd(cwd_buffer, sizeof(cwd_buffer)) != NULL)
  {
    fprintf(output, "%s\n", cwd_buffer);
  [cite_start]} [cite: 91]
  else
  {
    perror("getcwd");
  }

  if (output != stdout)
    fclose(output);
  [cite_start]if (error != stderr) [cite: 92]
    fclose(error);
}

/**
 * Execute built-in commands (echo, pwd, type) with optional redirection.
 * Returns 1 if a builtin was executed, 0 otherwise.
 * redirect_idx is used by echo to know where arguments end. Pass -1 if not applicable.
 */
int handle_builtin_command(char **args, int redirect_idx, char *redirect_stdout, char *redirect_stderr, int append_stdout, int append_stderr)
{
  // Check for 'pwd'
  if (strcmp(args[0], "pwd") == 0)
  {
    execute_pwd(redirect_stdout, redirect_stderr, append_stdout, append_stderr);
    return 1;
  }

  // Check for 'echo'
  if (strcmp(args[0], "echo") == 0)
  {
    execute_echo(args, redirect_idx, redirect_stdout, redirect_stderr, append_stdout, append_stderr);
    return 1;
  }

  // Check for 'type'
  if (strcmp(args[0], "type") == 0)
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
        return 1; // It was a builtin, but it failed
      }
    }
    // Handle stderr redirection
    if (redirect_stderr != NULL)
    {
      const char *mode = append_stderr ? "a" : "w";
      error = fopen(redirect_stderr, mode);
      if (error == NULL)
      {
        perror("fopen");
        if (output != stdout)
          fclose(output);
        return 1; // It was a builtin, but it failed
      }
    }

    if (args[1] == NULL)
    {
      fprintf(error, "type: missing argument\n");
    }
    else
    {
      char *arg = args[1];
      if (strcmp(arg, "echo") == 0 || strcmp(arg, "exit") == 0 ||
          strcmp(arg, "type") == 0 || strcmp(arg, "pwd") == 0 ||
          strcmp(arg, "cd") == 0)
      {
        fprintf(output, "%s is a shell builtin\n", arg);
      }
      else
      {
        char full_path[MAX_PATH_LENGTH];
        if (find_executable(arg, full_path, MAX_PATH_LENGTH))
        {
          fprintf(output, "%s is %s\n", arg, full_path);
        }
        else
        {
          fprintf(output, "%s: not found\n", arg);
        }
      }
    }

    if (output != stdout)
      fclose(output);
    if (error != stderr)
      fclose(error);

    return 1;
  }

  return 0; // Not a builtin
}

int main(int argc, char *argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL);
  [cite_start]// Initialize readline [cite: 93]
  init_readline();

  while (1)
  {
    // 1. Read the user's input using readline
    char *input = readline("$ ");
    [cite_start]// Check for EOF (Ctrl+D) [cite: 94]
    if (input == NULL)
    {
      printf("\n");
      [cite_start]break; [cite: 95]
    }

    // 2. Handle empty input
    if (strlen(input) == 0)
    {
      free(input);
      [cite_start]continue; [cite: 96]
    }

    // Add to history
    add_history(input);
    [cite_start]// Copy input to command buffer for processing [cite: 97]
    char command[MAX_COMMAND_LENGTH];
    strncpy(command, input, MAX_COMMAND_LENGTH - 1);
    [cite_start]command[MAX_COMMAND_LENGTH - 1] = '\0'; [cite: 98]

    // Free the readline buffer
    free(input);
    [cite_start]// --- Parse command with quote handling --- [cite: 99]
    char *args[MAX_ARGS];
    int arg_index = 0;
    [cite_start]char *read_ptr = command; [cite: 100]
    char *write_ptr = command;

    int state = STATE_DEFAULT;
    args[arg_index] = write_ptr;
    int new_arg = 1;
    [cite_start]while (*read_ptr != '\0') [cite: 101]
    {
      char c = *read_ptr;
      [cite_start]if (state == STATE_DEFAULT) [cite: 102]
      {
        if (c == '\\')
        {
          read_ptr++;
          [cite_start]if (*read_ptr == '\0') [cite: 103]
            break;
          *write_ptr = *read_ptr;
          write_ptr++;
          [cite_start]read_ptr++; [cite: 104]
          new_arg = 0;
        }
        else if (c == ' ')
        {
          if (!new_arg)
          {
            *write_ptr = '\0';
            [cite_start]write_ptr++; [cite: 105]

            arg_index++;
            if (arg_index >= MAX_ARGS - 1)
            {
              fprintf(stderr, "Error: Too many arguments\n");
              [cite_start]break; [cite: 106]
            }
            args[arg_index] = write_ptr;
            new_arg = 1;
          [cite_start]} [cite: 107]
          read_ptr++;
        [cite_start]} [cite: 108]
        else if (c == '\'')
        {
          state = STATE_IN_QUOTE;
          [cite_start]read_ptr++; [cite: 109]
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

          [cite_start]write_ptr++; [cite: 110]
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

        [cite_start]} [cite: 111]
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

        [cite_start]{ [cite: 112]
          if (read_ptr[1] == '\\' || read_ptr[1] == '"')
          {
            read_ptr++;
            [cite_start]*write_ptr = *read_ptr; [cite: 113]
            write_ptr++;
            read_ptr++;
          }
          else
          {
            *write_ptr = c;
            [cite_start]write_ptr++; [cite: 114]
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

          [cite_start]read_ptr++; [cite: 115]
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

    [cite_start]{ [cite: 116]
      arg_index++;
    }
    args[arg_index] = NULL;

    if (args[0] == NULL)
    {
      continue;
    }

    // --- Check for output redirection ---
    char *redirect_stdout = NULL;
    char *redirect_stderr = NULL;
    int append_stdout = 0;
    int append_stderr = 0;
    int redirect_idx = find_redirect(args, &redirect_stdout, &redirect_stderr, &append_stdout, &append_stderr);

    if (redirect_idx == -2)

    [cite_start]{ [cite: 117]
      continue; // Error in redirection syntax
    }

    // Null-terminate args at first redirect operator
    if (redirect_idx >= 0)
    {
      args[redirect_idx] = NULL;
    [cite_start]} [cite: 118]

    // --- Builtin Command Handling ---

    // 5. Check for 'exit 0' (must be handled by parent)
    if (strcmp(args[0], "exit") == 0)
    {
      return 0;
    }

    // --- Check for Pipeline ---
    int pipe_idx = find_pipe(args);
    if (pipe_idx > 0)
    {
      // Split the command into two parts at the pipe
      char *cmd1_args[MAX_ARGS];
      char *cmd2_args[MAX_ARGS];

      // Copy first command arguments
      for (int i = 0; i < pipe_idx; i++)
      {
        cmd1_args[i] = args[i];
      }
      cmd1_args[pipe_idx] = NULL;

      // Copy second command arguments
      int j = 0;
      for (int i = pipe_idx + 1; args[i] != NULL; i++)
      {
        cmd2_args[j++] = args[i];
      }
      cmd2_args[j] = NULL;

      // Check for empty pipe side
      if (cmd1_args[0] == NULL || cmd2_args[0] == NULL)
      {
        fprintf(stderr, "Error: Invalid null command in pipeline\n");
        continue;
      }

      // Create pipe
      int pipefd[2];
      if (pipe(pipefd) == -1)
      {
        perror("pipe");
        continue;
      }

      // Fork first child for first command
      pid_t pid1 = fork();
      if (pid1 == -1)
      {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        continue;
      }
      else if (pid1 == 0)
      {
        // First child: execute first command
        // Close read end of pipe
        close(pipefd[0]);
        // Redirect stdout to pipe write end
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
          perror("dup2");
          exit(EXIT_FAILURE);
        }
        close(pipefd[1]);

        // Check if command 1 is a builtin
        if (handle_builtin_command(cmd1_args, -1, NULL, NULL, 0, 0))
        {
          exit(EXIT_SUCCESS); // Builtin executed, exit child
        }

        // Not a builtin, find and exec external command
        char full_path1[MAX_PATH_LENGTH];
        if (find_executable(cmd1_args[0], full_path1, MAX_PATH_LENGTH))
        {
          if (execv(full_path1, cmd1_args) == -1)
          {
            perror("execv");
            exit(EXIT_FAILURE);
          }
        }
        else
        {
          fprintf(stderr, "%s: command not found\n", cmd1_args[0]);
          exit(EXIT_FAILURE);
        }
      }

      // Fork second child for second command
      pid_t pid2 = fork();
      if (pid2 == -1)
      {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0);
        continue;
      }
      else if (pid2 == 0)
      {
        // Second child: execute second command
        // Close write end of pipe
        close(pipefd[1]);
        // Redirect stdin to pipe read end
        if (dup2(pipefd[0], STDIN_FILENO) == -1)
        {
          perror("dup2");
          exit(EXIT_FAILURE);
        }
        close(pipefd[0]);

        // Check if command 2 is a builtin
        // Note: Redirection is handled by the shell for the *final* command,
        // but builtins inside a pipe don't get the main redirection args.
        if (handle_builtin_command(cmd2_args, -1, NULL, NULL, 0, 0))
        {
          exit(EXIT_SUCCESS); // Builtin executed, exit child
        }

        // Not a builtin, find and exec external command
        char full_path2[MAX_PATH_LENGTH];
        if (find_executable(cmd2_args[0], full_path2, MAX_PATH_LENGTH))
        {
          if (execv(full_path2, cmd2_args) == -1)
          {
            perror("execv");
            exit(EXIT_FAILURE);
          }
        }
        else
        {
          fprintf(stderr, "%s: command not found\n", cmd2_args[0]);
          exit(EXIT_FAILURE);
        }
      }

      // Parent: close both ends of pipe and wait for both children
      close(pipefd[0]);
      close(pipefd[1]);

      waitpid(pid1, NULL, 0);
      waitpid(pid2, NULL, 0);

      continue;
    }

    // --- No Pipeline ---

    // 8. Check for 'cd' (must be handled by parent)
    if (strcmp(args[0], "cd") == 0)
    {
      if (args[1] == NULL)
      {
        fprintf(stderr, "cd: missing argument\n");
        continue;
      }

      char *path_to_change = NULL;
      if (strcmp(args[1], "~") == 0)
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
        path_to_change = args[1];
      }

      if (chdir(path_to_change) != 0)
      {
        fprintf(stderr, "cd: %s: %s\n", args[1], strerror(errno));
      }
      continue;
    }

    // Check for other builtins (echo, pwd, type)
    if (handle_builtin_command(args, redirect_idx, redirect_stdout, redirect_stderr, append_stdout, append_stderr))
    {
      continue;
    }

    // --- External Command Execution (No Pipeline) ---
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

        // Handle stdout redirection
        if (redirect_stdout != NULL)
        {
          int flags = O_WRONLY |
                      O_CREAT;
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
          int flags = O_WRONLY |
                      O_CREAT;
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