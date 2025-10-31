#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // For strcmp, strncmp, strlen, strdup, strtok, strerror
#include <unistd.h>    // For access(), X_OK, fork(), execv(), getcwd(), chdir(), STDOUT_FILENO, dup2, close
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

    // --- (NEW PARSER) Parse command *before* checking builtins ---
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
          read_ptr++; // Consume backslash
          if (*read_ptr == '\0')
            break;
          *write_ptr = *read_ptr;
          write_ptr++;
          read_ptr++;
          new_arg = 0;
        }
        else if (c == ' ')
        { // Delimiter
          if (!new_arg)
          {
            *write_ptr = '\0';
            write_ptr++;
            arg_index++;
            if (arg_index >= MAX_ARGS - 1) break;
            args[arg_index] = write_ptr;
            new_arg = 1;
          }
          read_ptr++; // Skip space
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
        // --- NEW: Handle redirection operators as delimiters ---
        else if (c == '>')
        { // Case: `>`
          if (!new_arg)
          { // Terminate previous arg
            *write_ptr = '\0';
            write_ptr++;
            arg_index++;
            if (arg_index >= MAX_ARGS - 1) break;
          }
          // Add ">" as its own argument
          *write_ptr = '>';
          write_ptr++;
          *write_ptr = '\0';
          write_ptr++;
          args[arg_index] = write_ptr - 2; // Point to the ">"
          arg_index++;
          if (arg_index >= MAX_ARGS - 1) break;
          new_arg = 1; // Ready for next arg
          read_ptr++;  // Consume ">"
        }
        else if (c == '1' && read_ptr[1] == '>')
        { // Case: `1>`
          if (!new_arg)
          { // Terminate previous arg
            *write_ptr = '\0';
            write_ptr++;
            arg_index++;
            if (arg_index >= MAX_ARGS - 1) break;
          }
          // Add "1>" as its own argument
          *write_ptr = '1';
          write_ptr++;
          *write_ptr = '>';
          write_ptr++;
          *write_ptr = '\0';
          write_ptr++;
          args[arg_index] = write_ptr - 3; // Point to the "1>"
          arg_index++;
          if (arg_index >= MAX_ARGS - 1) break;
          new_arg = 1; // Ready for next arg
          read_ptr += 2; // Consume "1>"
        }
        // --- End of new logic ---
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
    } // end parser while

    if (state != STATE_DEFAULT)
    {
      fprintf(stderr, "Error: Unclosed quote\n");
      continue;
    }
    *write_ptr = '\0'; // Null-terminate the last argument
    if (!new_arg)
    {
      arg_index++;
    }
    args[arg_index] = NULL;

    // --- (NEW) Post-Parsing: Separate args from redirection ---
    char *real_args[MAX_ARGS];
    char *output_file = NULL;
    int real_arg_count = 0;
    int parse_error = 0;

    for (int i = 0; args[i] != NULL; i++)
    {
      if (strcmp(args[i], ">") == 0 || strcmp(args[i], "1>") == 0)
      {
        if (args[i + 1] != NULL)
        {
          output_file = args[i + 1];
          i++; // Skip the filename, it's not a real arg
        }
        else
        {
          fprintf(stderr, "shell: syntax error near unexpected token `newline'\n");
          parse_error = 1;
          break;
        }
      }
      else
      {
        real_args[real_arg_count] = args[i];
        real_arg_count++;
      }
    }
    real_args[real_arg_count] = NULL;

    if (parse_error) continue;
    if (real_args[0] == NULL) continue; // Empty command

    // --- (REFACTORED) Builtin Handling ---

    // 5. Handle 'exit' (non-forked)
    if (strcmp(real_args[0], "exit") == 0)
    {
      if (real_args[1] && strcmp(real_args[1], "0") == 0)
      {
        return 0; // Exit
      }
      // Handle other exit codes later if needed
      return 0;
    }

    // 6. Handle 'cd' (non-forked)
    if (strcmp(real_args[0], "cd") == 0)
    {
      char *path_to_change = NULL;
      if (real_args[1] == NULL) {
          path_to_change = getenv("HOME");
      } else if (strcmp(real_args[1], "~") == 0) {
          path_to_change = getenv("HOME");
      } else {
          path_to_change = real_args[1];
      }
      
      if (path_to_change == NULL) {
          fprintf(stderr, "cd: HOME not set\n");
      } else if (chdir(path_to_change) != 0) {
          fprintf(stderr, "cd: %s: %s\n", real_args[1], strerror(errno));
      }
      continue; // 'cd' is done, loop back
    }

    // --- (NEW) Fork for *all* other commands (builtins & external) ---

    pid_t pid = fork();

    if (pid == -1)
    {
      perror("fork");
    }
    else if (pid == 0)
    {
      // --- This is the Child Process ---

      // 7. Handle Redirection
      if (output_file != NULL)
      {
        // Open the file
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
          perror("open");
          exit(EXIT_FAILURE);
        }
        
        // Redirect stdout (FD 1) to the file
        if (dup2(fd, STDOUT_FILENO) == -1) {
          perror("dup2");
          exit(EXIT_FAILURE);
        }
        close(fd); // We don't need the original fd anymore
      }

      // 8. Handle *forked* builtins
      if (strcmp(real_args[0], "pwd") == 0)
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
        exit(0); // Exit child
      }

      if (strcmp(real_args[0], "type") == 0)
      {
        char* arg = real_args[1];
        if (arg == NULL) {
            exit(0); // 'type' with no args
        }
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
        exit(0); // Exit child
      }

      // 9. Handle External Commands
      char full_path[MAX_PATH_LENGTH];
      if (find_executable(real_args[0], full_path, MAX_PATH_LENGTH))
      {
        if (execv(full_path, real_args) == -1)
        {
          perror("execv");
          exit(EXIT_FAILURE);
        }
      }
      else
      {
        fprintf(stderr, "%s: command not found\n", real_args[0]);
        exit(127); // Standard exit code for "command not found"
      }
    }
    else
    {
      // --- This is the Parent Process ---
      int status;
      waitpid(pid, &status, 0);
    }
  }

  return 0;
}