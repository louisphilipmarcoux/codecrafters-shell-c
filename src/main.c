#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define F_OK 0
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

void parse_input(const char *input, char *cmd, char **args, int *argc);
bool find_executable(const char *cmd, char *result_path);
void run_executable(const char *cmd, char **args);

int main()
{

  // Process user input
  bool running = true;
  char input[100];
  char cmd[100];
  char *args[10];
  int argc;

  while (running)
  {
    // Flush after every printf
    setbuf(stdout, NULL);
    printf("$ ");

    // Exit on error
    if (fgets(input, sizeof(input), stdin) == NULL)
    {
      running = false;
    }

    // remove \n
    int n = strlen(input);
    if (n > 0 && input[n - 1] == '\n')
    {
      input[n - 1] = '\0';
    }

    // Parse input
    parse_input(input, cmd, args, &argc);

    // Evaluate commands:
    if (!strcmp(cmd, "exit"))
    {
      // printf("Exiting...\n");
      running = false;
    }
    else if (!strcmp(cmd, "echo"))
    {
      for (int i = 0; i < argc; i++)
      {
        printf("%s ", args[i]);
      }
      printf("\n");
    }
    else if (!strcmp(cmd, "type"))
    {
      if (argc > 0)
      {
        char path[512];
        if (!strcmp(args[0], "echo") || !strcmp(args[0], "exit") || !strcmp(args[0], "type"))
        {
          printf("%s is a shell builtin\n", args[0]);
        }
        else if (find_executable(args[0], path))
        {
          printf("%s is %s\n", args[0], path);
        }
        else
        {
          printf("%s not found\n", args[0]);
        }
      }
      else
      {
        printf("Usage: type [command]\n");
      }
    }
    else
    {
      char path[512];
      if (find_executable(cmd, path))
      {
        run_executable(path, args);
      }
      else
      {
        printf("%s: command not found\n", cmd);
      }
    }

    // free mem
    for (int i = 0; i < argc; i++)
    {
      free(args[i]);
    }
  }
  return 0;
}

void parse_input(const char *input, char *cmd, char **args, int *argc)
{

  char input_cpy[100];
  strncpy(input_cpy, input, sizeof(input_cpy) - 1);
  input_cpy[sizeof(input_cpy) - 1] = '\0';

  char *token = strtok(input_cpy, " ");
  if (token != NULL)
  {
    strcpy(cmd, token);
  }

  *argc = 0;
  while ((token = strtok(NULL, " ")) != NULL)
  {
    args[*argc] = malloc(strlen(token) + 1);
    if (args[*argc] != NULL)
    {
      strcpy(args[*argc], token);
      (*argc)++;
    }
  }

  args[*argc] = NULL;
}

bool find_executable(const char *cmd, char *result_path)
{

// First, search in current folder
#ifdef _WIN32
  const char *extensions[] = {".exe", NULL}; // Extension support list
  // ------------
  if (strchr(cmd, '.'))
  {
    snprintf(result_path, 512, ".\\%s", cmd);
    if (access(result_path, F_OK) == 0)
    {
      return true;
    }
  }
  else
  {
    for (const char **ext = extensions; *ext != NULL; ext++)
    {
      snprintf(result_path, 512, ".\\%s%s", cmd, *ext);
      if (access(result_path, F_OK) == 0)
      {
        return true;
      }
    }
  }

#else
  // In POSIX systems, simply search in current folder
  snprintf(result_path, 512, "./%s", cmd);
  if (access(result_path, F_OK) == 0)
  {
    return true;
  }
#endif

    // Second, search in PATH
    char *path_env = getenv("PATH");
    if (!path_env)
    {
      return false;
    }

    char path_copy[1024];
    strncpy(path_copy, path_env, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char separator = ':';
#ifdef _WIN32
    separator = ';';
#endif

    char *dir = strtok(path_copy, &separator);
    while (dir != NULL)
    {
      if (strlen(dir) > 0)
      {
#ifdef _WIN32
        if (strchr(cmd, '.'))
        {
          snprintf(result_path, 512, "%s\\%s", dir, cmd);
          if (access(result_path, F_OK) == 0)
          {
            return true;
          }
        }
        else
        {
          for (const char **ext = extensions; *ext != NULL; ext++)
          {
            snprintf(result_path, 512, "%s\\%s%s", dir, cmd, *ext);
            if (access(result_path, F_OK) == 0)
            {
              return true;
            }
          }
        }
#else
      snprintf(result_path, 512, "%s/%s", dir, cmd);
      if (access(result_path, X_OK) == 0)
      {
        return true;
      }
#endif
      }
      dir = strtok(NULL, &separator);
    }

    return false;
  }

  void run_executable(const char *cmd, char **args)
  {
#ifdef _WIN32
    // 1. Build command line
    char command_line[1024] = {0};
    snprintf(command_line, sizeof(command_line), "\"%s\"", cmd);

    for (int i = 0; args[i] != NULL; i++)
    {
      strncat(command_line, " ", sizeof(command_line) - strlen(command_line) - 1);
      strncat(command_line, args[i], sizeof(command_line) - strlen(command_line) - 1);
    }

    // 2. Setup structures for 'CreateProcess'
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(STARTUPINFO);

    // 3. Try to create process
    if (!CreateProcess(NULL, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
      fprintf(stderr, "Error: error executing command %s\n", cmd);
    }
    else
    {
      WaitForSingleObject(pi.hProcess, INFINITE);
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    }

#else
  // Use 'execvp' in POSIX systems
  if (fork() == 0)
  {
    execvp(cmd, args);
    perror("Error executing command");
    exit(EXIT_FAILURE);
  }
  else
  {
    wait(NULL);
  }
#endif
  }