#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>

#define ENABLE_READLINE

#ifdef ENABLE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#else

// dummy definitions for debugging

typedef struct
{
  char *line;
} HIST_ENTRY;

HIST_ENTRY **history_list() { return NULL; }
void using_history() {}
void clear_history() {}
void add_history(char *s) {}

void *rl_completion_entry_function;

char *readline(char *prompt)
{
  printf("%s", prompt);
  size_t len;
  char *response = NULL;
  getline(&response, &len, stdin);
  return response;
}

#endif

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0]))
#define MAX_PATH_LENGTH 1024
#define MAX_CMD_INPUT 1024

bool exitShell = false;

char *builtins[] = {
    "exit",
    "echo",
    "type",
    "pwd",
    "cd",
    "history",
};

char *pathLookup(char *name)
{
  char fullPath[MAX_PATH_LENGTH];
  char *path = getenv("PATH");
  if (path == NULL)
  {
    return NULL;
  }
  for (;;)
  {
    char *separator = strchr(path, ':');
    int pathLen = separator != NULL ? separator - path : strlen(path);
    if (pathLen == 0)
    {
      break;
    }
    int fullPathLen = snprintf(fullPath, MAX_PATH_LENGTH, "%.*s/%s", pathLen, path, name);
    if (access(fullPath, F_OK) == 0)
    {
      char *result = malloc(fullPathLen + 1);
      if (result == NULL)
      {
        perror("malloc");
        return NULL;
      }
      snprintf(result, fullPathLen + 1, "%s", fullPath);
      return result;
    }
    if (separator == NULL)
    {
      break;
    }
    path = separator + 1;
  }
  return NULL;
}

void cmdType(char *arg, FILE *out, FILE *err)
{
  for (int i = 0; i < ARRAY_LENGTH(builtins); i++)
  {
    if (strcmp(arg, builtins[i]) == 0)
    {
      fprintf(out, "%s is a shell builtin\n", arg);
      return;
    }
  }
  char *fullPath = pathLookup(arg);
  if (fullPath != NULL)
  {
    fprintf(out, "%s is %s\n", arg, fullPath);
    free(fullPath);
    return;
  }
  fprintf(err, "%s: not found\n", arg);
}

char **splitCommandLine(char *input)
{
  int count = 0;
  int capacity = 0;
  char **result = NULL;
  char token[MAX_CMD_INPUT];
  int length = 0;
  char *cur = input;
  for (;;)
  {
    bool panic = false;
    switch (cur[0])
    {
    case '\'':
    {
      cur++; // opening '
      while (cur[0] != '\'' && cur[0] != '\0')
      {
        token[length++] = cur[0];
        cur++;
      }
      if (cur[0] == '\0')
      {
        fprintf(stderr, "unmatched '\n");
        panic = true;
        break;
      }
      cur++; // closing '
      break;
    }
    case '"':
    {
      cur++; // opening "
      while (cur[0] != '"' && cur[0] != '\0')
      {
        if (cur[0] == '\\' && (cur[1] == '\\' || cur[1] == '$' || cur[1] == '"'))
        {
          token[length++] = cur[1];
          cur += 2;
        }
        else
        {
          token[length++] = cur[0];
          cur++;
        }
      }
      if (cur[0] == '\0')
      {
        panic = true;
        fprintf(stderr, "unmatched \"\n");
        break;
      }
      cur++; // closing "
      break;
    }
    case '\\':
    {
      token[length++] = cur[1];
      cur += 2;
      break;
    }
    default:
      if (!isspace(cur[0]) && cur[0] != '\0')
      {
        token[length++] = cur[0];
        cur++;
      }
    }
    if (panic)
    {
      for (int i = 0; i < count; i++)
      {
        free(result[i]);
      }
      free(result);
      return NULL;
    }
    // check if reached token boundary
    if (isspace(cur[0]) || cur[0] == '\0')
    {
      while (isspace(cur[0]))
      {
        cur++;
      }
      if (capacity < count + 1)
      {
        capacity = capacity == 0 ? 8 : capacity * 2;
        result = realloc(result, sizeof(char *) * capacity);
        if (result == NULL)
        {
          perror("realloc");
          exit(EXIT_FAILURE);
        }
      }
      char *arg = malloc(length + 1);
      if (arg == NULL)
      {
        perror("malloc");
        exit(EXIT_FAILURE);
      }
      snprintf(arg, length + 1, "%s", token);
      result[count++] = arg;
      length = 0;
      if (cur[0] == '\0')
      {
        break;
      }
    }
  }
  if (count == 0)
  {
    return NULL;
  }
  if (capacity < count + 1)
  {
    capacity++;
    result = realloc(result, sizeof(char *) * capacity);
    if (result == NULL)
    {
      perror("realloc");
      exit(EXIT_FAILURE);
    }
  }
  result[count++] = NULL;
  return result;
}

void freeArrayAndElements(char **array)
{
  for (int i = 0; array[i] != NULL; i++)
  {
    free(array[i]);
  }
  free(array);
}

void childRedir(FILE *stream, int fd)
{
  if (stream == stdin || stream == stdout || stream == stderr)
  {
    return;
  }
  int fdOfStream = fileno(stream);
  dup2(fdOfStream, fd);
  close(fdOfStream);
}

void safeClose(FILE *file)
{
  if (file == stdin || file == stdout || file == stderr)
  {
    return;
  }
  fclose(file);
}

void callExecutable(char *cmd, char **args, bool shouldWait, FILE *in, FILE *out, FILE *err)
{
  int pid = fork();
  if (pid == 0)
  {
    childRedir(in, 0);
    childRedir(out, 1);
    childRedir(err, 2);
    execv(cmd, args);
    perror("execv");
    _exit(EXIT_FAILURE); // child should not return
  }
  else if (pid > 0)
  {
    if (shouldWait)
    {
      int status;
      waitpid(pid, &status, WUNTRACED);
    }
  }
  else
  {
    perror("fork");
  }
}

int min(int a, int b)
{
  return a < b ? a : b;
}

bool handleRedirection(char **args, FILE **out, FILE **err)
{
  // TODO: implement < redirection
  int firstRedirectIndex = INT_MAX;
  for (int i = 0; args[i] != NULL; i++)
  {
    if (strcmp(args[i], "1>") == 0 || strcmp(args[i], ">") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "syntax error after %s\n", args[i]);
        return false;
      }
      *out = fopen(args[i + 1], "w");
      if (*out == NULL)
      {
        perror("fopen");
        return false;
      }
      firstRedirectIndex = min(i, firstRedirectIndex);
    }
    else if (strcmp(args[i], "1>>") == 0 || strcmp(args[i], ">>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "syntax error after %s\n", args[i]);
        return false;
      }
      *out = fopen(args[i + 1], "a");
      if (*out == NULL)
      {
        perror("fopen");
        return false;
      }
      firstRedirectIndex = min(i, firstRedirectIndex);
    }
    else if (strcmp(args[i], "2>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "syntax error after %s\n", args[i]);
        return false;
      }
      *err = fopen(args[i + 1], "w");
      if (*err == NULL)
      {
        perror("fopen");
        return false;
      }
      firstRedirectIndex = min(i, firstRedirectIndex);
    }
    else if (strcmp(args[i], "2>>") == 0)
    {
      if (args[i + 1] == NULL)
      {
        fprintf(stderr, "syntax error after %s\n", args[i]);
        return false;
      }
      *err = fopen(args[i + 1], "a");
      if (*err == NULL)
      {
        perror("fopen");
        return false;
      }
      firstRedirectIndex = min(i, firstRedirectIndex);
    }
  }
  if (firstRedirectIndex != INT_MAX)
  {
    // TODO: consider allocating a copy instead and freeing the original
    // free rest of array and mark new with null;
    for (int i = firstRedirectIndex; args[i] != NULL; i++)
    {
      free(args[i]);
    }
    args[firstRedirectIndex] = NULL;
  }
  return true;
}

void handleCmd(char *cmd, char **args, bool shouldWait, FILE *in, FILE *out, FILE *err)
{
  if (!handleRedirection(args, &out, &err))
  {
    return;
  }
  if (strcmp(cmd, "exit") == 0)
  {
    exitShell = true;
  }
  else if (strcmp(cmd, "echo") == 0)
  {
    for (int i = 1; args[i] != NULL; i++)
    {
      fprintf(out, "%s%s", i == 1 ? "" : " ", args[i]);
    }
    fprintf(out, "\n");
  }
  else if (strcmp(cmd, "type") == 0)
  {
    if (args[1] != NULL)
    {
      cmdType(args[1], out, err);
    }
  }
  else if (strcmp(cmd, "pwd") == 0)
  {
    char currentDir[MAX_PATH_LENGTH];
    getcwd(currentDir, MAX_PATH_LENGTH);
    fprintf(out, "%s\n", currentDir);
  }
  else if (strcmp(cmd, "cd") == 0)
  {
    char *path = args[1];
    if (path != NULL)
    {
      if (strcmp("~", path) == 0)
      {
        path = getenv("HOME");
      }
      if (chdir(path) != 0)
      {
        fprintf(err, "cd: %s: %s\n", path, strerror(errno));
      }
    }
  }
  else if (strcmp(cmd, "history") == 0)
  {
    int start = 0;
    HIST_ENTRY **entries = history_list();
    if (args[1] != NULL)
    {
      int n = atoi(args[1]);
      for (int i = 0; entries[i] != NULL; i++)
      {
        start++;
      }
      start -= n;
      if (start < 0)
      {
        start = 0;
      }
    }
    for (int i = start; entries[i] != NULL; i++)
    {
      printf("%d %s\n", i + 1, entries[i]->line);
    }
  }
  else
  {
    char *fullPath = pathLookup(cmd);
    if (fullPath != NULL)
    {
      callExecutable(fullPath, args, shouldWait, in, out, err);
      free(fullPath);
    }
    else
    {
      fprintf(err, "%s: command not found\n", cmd);
    }
  }
  safeClose(in);
  safeClose(out);
  safeClose(err);
}

char **completionEntries = NULL;
int completionEntriesCount = 0;
int completionEntriesCapacity = 0;

char *nextCompletionEntryCallback(const char *text, int state)
{
  static int index, length;
  if (state == 0)
  {
    index = 0;
    length = strlen(text);
  }
  // TODO: since entries are sorted, could possible binary search to find first matching entry
  while (index < completionEntriesCount)
  {
    int current = index++;
    if (strncmp(completionEntries[current], text, length) == 0)
    {
      return strdup(completionEntries[current]);
    }
  }
  return NULL;
}

void addCompletionEntry(char *name)
{
  if (completionEntriesCapacity < completionEntriesCount + 1)
  {
    completionEntriesCapacity = completionEntriesCapacity == 0 ? 8 : completionEntriesCapacity * 2;
    completionEntries = realloc(completionEntries, sizeof(completionEntries[0]) * completionEntriesCapacity);
    if (completionEntries == NULL)
    {
      exit(EXIT_FAILURE);
    }
  }
  char *clone = strdup(name);
  completionEntries[completionEntriesCount++] = clone;
}

void freeCompletionEntries()
{
  for (int i = 0; i < completionEntriesCount; i++)
  {
    free(completionEntries[i]);
  }
  free(completionEntries);
  completionEntriesCount = 0;
  completionEntriesCapacity = 0;
  completionEntries = NULL;
}

int entryCompare(const void *pp1, const void *pp2)
{
  char const *s1 = *(const char **)pp1;
  char const *s2 = *(const char **)pp2;
  return strcmp(s1, s2);
}

void printCompletionEntries()
{
  for (int i = 0; i < completionEntriesCount; i++)
  {
    printf("%d: %s\n", i, completionEntries[i]);
  }
}

void removeDuplicateEntries()
{
  // BUG: test this more thoroughly
  int writeIndex = 1;
  for (int readIndex = 1; readIndex < completionEntriesCount; readIndex++)
  {
    if (entryCompare(&completionEntries[readIndex], &completionEntries[writeIndex - 1]) != 0)
    {
      if (readIndex != writeIndex)
      {
        completionEntries[writeIndex] = completionEntries[readIndex];
      }
      writeIndex++;
    }
    else
    {
      free(completionEntries[readIndex]);
    }
  }
  for (int i = writeIndex; i < completionEntriesCount; i++)
  {
    completionEntries[i] = NULL;
  }
  completionEntriesCount = writeIndex;
}

void sortCompletionEntries()
{
  qsort(completionEntries, completionEntriesCount, sizeof(completionEntries[0]), entryCompare);
}

void updateCompletionEntries()
{
  freeCompletionEntries();
  char *path = getenv("PATH");
  char pathStr[MAX_PATH_LENGTH];
  if (path == NULL)
  {
    return;
  }
  for (;;)
  {
    char *separator = strchr(path, ':');
    int pathLen = separator != NULL ? separator - path : strlen(path);
    if (pathLen == 0)
    {
      break;
    }
    snprintf(pathStr, MAX_PATH_LENGTH, "%.*s", pathLen, path);
    // printf("Searching path: %s\n", pathStr);
    DIR *dir = opendir(pathStr);
    if (dir != NULL)
    {
      struct dirent *entry = readdir(dir);
      while (entry != NULL)
      {
        addCompletionEntry(entry->d_name);
        entry = readdir(dir);
      }
      closedir(dir);
    }
    if (separator == NULL)
    {
      break;
    }
    path = separator + 1;
  }
  for (int i = 0; i < ARRAY_LENGTH(builtins); i++)
  {
    addCompletionEntry(builtins[i]);
  }
  // TODO: test these thoroughly and re-enable them
  // sortCompletionEntries();
  // removeDuplicateEntries();
}

typedef struct
{
  char **args;
} CommandArgs;

typedef struct
{
  int count;
  CommandArgs *commands;
} CommandGroup;

CommandGroup splitPipes(char **args)
{
  CommandGroup group = {};
  int prevPipeIndex = -1;
  for (int i = 0;; i++)
  {
    if (args[i] == NULL || strcmp(args[i], "|") == 0)
    {
      group.count++;
    }
    if (args[i] == NULL)
    {
      break;
    }
  }
  group.commands = malloc(group.count * sizeof(CommandArgs));
  if (group.commands == NULL)
  {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  int cmdIndex = 0;
  for (int i = 0;; i++)
  {
    if (args[i] == NULL || strcmp(args[i], "|") == 0)
    {
      if (args[i] != NULL && args[i + 1] == NULL)
      {
        fprintf(stderr, "syntax error after %s\n", args[i]);
        // cleanup allocated commands and args
        for (int i = 0; i < group.count; i++)
        {
          freeArrayAndElements(group.commands[i].args);
        }
        free(group.commands);
        group.commands = NULL;
        group.count = 0;
        break;
      }
      CommandArgs *cmd = &group.commands[cmdIndex++];
      int argCount = i - prevPipeIndex + 1; // NULL terminator
      int argIndex = 0;
      cmd->args = malloc(sizeof(char *) * argCount);
      if (cmd->args == NULL)
      {
        perror("malloc");
        exit(EXIT_FAILURE);
      }
      // printf("cmd %d:", cmdIndex - 1);
      for (int j = prevPipeIndex + 1; j < i; j++)
      {
        // printf(" %d:%s", argIndex, args[j]);
        cmd->args[argIndex++] = strdup(args[j]);
      }
      // printf("\n");
      cmd->args[argIndex++] = NULL;
      prevPipeIndex = i;
    }
    if (args[i] == NULL)
    {
      break;
    }
  }
  freeArrayAndElements(args);
  return group;
}

int main(int argc, char *argv[])
{
  // Flush after every printf
  setbuf(stdout, NULL);
  updateCompletionEntries();
  rl_completion_entry_function = nextCompletionEntryCallback;
  using_history();
  while (!exitShell)
  {
    char *input = readline("$ ");
    if (input == NULL)
    {
      break;
    }
    char **args = splitCommandLine(input);
    if (args == NULL || args[0] == NULL || strlen(args[0]) == 0)
    {
      continue;
    }
    add_history(input);
    CommandGroup group = splitPipes(args);
    if (group.count == 1)
    {
      char **args = group.commands[0].args;
      handleCmd(args[0], args, true, stdin, stdout, stderr);
      freeArrayAndElements(args);
    }
    else
    {
      FILE *input, *output, *prevInput;
      prevInput = stdin;
      for (int i = 0; i < group.count; i++)
      {
        bool shouldWait = false;
        if (i < group.count - 1)
        {
          int pipeDescriptors[2];
          if (pipe(pipeDescriptors) != 0)
          {
            perror("pipe");
            return EXIT_FAILURE;
          }
          input = fdopen(pipeDescriptors[0], "r");
          output = fdopen(pipeDescriptors[1], "w");
        }
        else
        {
          output = stdout;
          shouldWait = true;
        }
        char **args = group.commands[i].args;
        handleCmd(args[0], args, shouldWait, prevInput, output, stderr);
        freeArrayAndElements(args);
        prevInput = input;
      }
    }
    free(group.commands);
    free(input);
  }
  clear_history();
  freeCompletionEntries();
  return 0;
}