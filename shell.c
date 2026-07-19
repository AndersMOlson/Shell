// Replace everything with "Replace" or "Remove"

/*
  Part 0: Libraries
*/
#include <sys/wait.h>
#include <sys/types.h>
/* 
  waitpid() and associated macros
*/

#include <termios.h>
/*
  termios
*/

#include <signal.h>
/*
  signal()
  SIGINT
  SIGTSTP
*/

#include <unistd.h>
/*
  chdir()
  fork()
  exec()
  pipe()
  pid_t
*/

#include <stdlib.h>
#include <stdbool.h>
/* 
  calloc()
  malloc()
  realloc()
  free()
  exit()
  execvp()
  EXIT_SUCCESS, EXIT_FAILURE
*/

#include <stdio.h>
/* 
  fprintf()
  printf()
  stderr
  getchar()
  perror()
*/

#include <fcntl.h>
/*
  open()
  O_RDONLY
  O_WRONLY
  O_CREAT
  O_TRUNC
  O_APPEND
*/

#include <errno.h>
/*
  errno
  ECHILD
*/

#include <string.h>
/*
  strcmp()
  strtok()
  strchr()
  strdup()
*/

// Custom Libraries
#include "job.h"





/*
  Part 1: Functions and Implementations
*/

// Function Declarations for builtin shell commands:
int aos_cd(char **args);
int aos_help(char **args);
int aos_jobs(char **args);
int aos_fg(char **args);
int aos_bg(char **args);
int aos_alias(char **args);
int aos_unalias(char **args);
int aos_history(char **args);
int aos_exit(char **args);
pid_t shell_pgid;

// Forward Declaration for later funcitons
typedef struct {
  char *value;
  bool quoted;
} Token;

Token *aos_tokenize_line(char *line);

// List of builtin commands, followed by their corresponding functions.
char *builtin_str[] = {
  "cd",
  "help",
  "jobs",
  "fg",
  "bg",
  "alias",
  "unalias",
  "history",
  "exit"
};

int (*builtin_func[]) (char **) = {
  &aos_cd,
  &aos_help,
  &aos_jobs,
  &aos_fg,
  &aos_bg,
  &aos_alias,
  &aos_unalias,
  &aos_history,
  &aos_exit
};

int aos_num_builtins() 
{
  return sizeof(builtin_str) / sizeof(char *);
}

char *aos_strdup_check(const char *str){
  char *dup = strdup(str);
  if (!dup) {
    perror("strdup() Failed");
    exit(EXIT_FAILURE);
  }
  return dup;
}

/*
  Builtin function implementations.
*/
#define MAX_ALIAS 64
#define MAX_HISTORY 256

typedef struct
{
  char *name;
  char *value;
} Alias;

static char *history_list[MAX_HISTORY];

static Alias aliases[MAX_ALIAS];

int aos_cd(char **args) 
{
  if (args[1] == NULL) {
    fprintf(stderr, "aos: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("aos");
    }
  }
  return 1;
}

int aos_help(char **args) 
{
  int i;
  printf("My Custom C Shell\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < aos_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("The shell supports: \n- Pipes (|)\n- Redirection (<, >, and >>)\n- Enviornment variables ($) and asssignment (=)\n- Background Jobs (&)\n");
  printf("Use the man command for information on other programs.\n");
  return 1;
}

int aos_jobs(char **args)
{
  jobs_print();
  return 1;
}

int aos_fg(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "fg: expected job id\n");
    return 1;
  }

  int id = atoi(args[1]);
  Job *job = jobs_find_by_id(id);
  if (job == NULL) {
    fprintf(stderr, "fg: no such job %d\n", id);
    return 1;
  }

  tcsetpgrp(STDIN_FILENO, job->pgid);
  kill(-job->pgid, SIGCONT);
  int status;
  waitpid(-job->pgid, &status, WUNTRACED);
  tcsetpgrp(STDIN_FILENO, shell_pgid);

  if (WIFSTOPPED(status)) {
    jobs_update_state(job->pgid, JOB_STOPPED);
  } else {
    jobs_remove(job->pgid);
  }
  return 1;
}

int aos_bg(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "bg: expected job id\n");
    return 1;
  }
  
  int id = atoi(args[1]);
  Job *job = jobs_find_by_id(id);
  if (job == NULL) {
    fprintf(stderr, "bg: no such job %d\n", id);
    return 1;
  }
  kill(-job->pgid, SIGCONT);
  jobs_update_state(job->pgid, JOB_RUNNING);
  return 1;
}

int aos_alias(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "alias: expected alias assignment\n");
    return 1;
  }

  if (strchr(args[1], '=') == NULL) {
    fprintf(stderr, "alias: Alias not formatted correctly (name=var)\n");
    return 1;
  }

  char *assignment = aos_strdup_check(args[1]);
  char *var_name = strtok(assignment, "=");
  char *var_value = strtok(NULL, "=");

  if (var_name == NULL || var_value == NULL) {
    fprintf(stderr, "alias: Alias not formatted correctly (name=var)\n");
    free(assignment);
    return 1;
  }

  for (int i = 0; i < MAX_ALIAS; i++) {
    if (aliases[i].name != NULL && strcmp(aliases[i].name, var_name) == 0) {
      free(aliases[i].value);
      aliases[i].value = aos_strdup_check(var_value);
      free(assignment);
      printf("Added alias %s=%s\n", aliases[i].name, aliases[i].value);
      return 1;
    }

    if (aliases[i].name == NULL) {
      aliases[i].name = aos_strdup_check(var_name);
      aliases[i].value = aos_strdup_check(var_value);
      free(assignment);
      printf("Added alias %s=%s\n", aliases[i].name, aliases[i].value);
      return 1;
    }
  }

  fprintf(stderr, "alias: Alias table full\n");
  free(assignment);
  return 1;
}

int aos_unalias(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "unalias: expected alias name to remove\n");
    return 1;
  }

  for (int i = 0; i < MAX_ALIAS; i++) {
    if (strcmp(aliases[i].name, args[1]) == 0) {
      aliases[i].name = NULL;
      aliases[i].value = NULL;
      printf("Removed alias %s", args[1]);
      return 1;
    }
  }

  fprintf(stderr, "unalias: No alias by that name\n");
  return 1;
}

int aos_history(char **args) {
  for (int i = 0; i < MAX_HISTORY; i++) {
    if (history_list[i] == NULL) {
      break;
    }
    printf("%d: %s\n", (i + 1), history_list[i]);
  }
  return 1;
}

int aos_exit(char **args) 
{
  return 0;
}





/*
  Part 2: Shell Command Toknization
*/
struct termios orig, raw;

void redraw_line(char *line)
{
  write(STDOUT_FILENO, "\033[2K\r", 5);
  char *cwd = getcwd(NULL, 0);
  printf("aos %s> ", cwd);
  fflush(stdout);
  free(cwd);
  write(STDOUT_FILENO, line, strlen(line));
}

char *aos_read_line(int history_index) 
{
  int current_index = history_index;
  char *line = calloc(2048, sizeof(char));
  size_t pos = 0;

  tcgetattr(STDIN_FILENO, &orig);

  raw = orig;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 1;   // Return after reading 1 byte
  raw.c_cc[VTIME] = 0;  // No timeout

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  while (1) {
    char c;

    if (read(STDIN_FILENO, &c, 1) != 1) {
      continue;
    }
    
    switch (c) {
      case '\n': // Enter
        printf("\n");
          goto done;

      case 127: // Backspace
        if (pos > 0) {
          pos--;
          line[pos] = '\0';
          write(STDOUT_FILENO, "\b \b", 3);
        }
        break;

      case 27:  // Arrow Keys
        char seq[2];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
          break;
        }
            
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
          break;
        }

        if (seq[0] == '[') {
          switch (seq[1]) {
            case 'A': // Up
              if (current_index > 0) {          
                current_index--;
                strcpy(line, history_list[current_index]);
                pos = strlen(line);
                redraw_line(line);
                }
              break;

            case 'B': // Down
              if (current_index < (history_index - 1)) {
                current_index++;
                strcpy(line, history_list[current_index]);
                pos = strlen(line);
              } else {
                current_index = history_index;
                line[0] = '\0';
                pos = 0;
              }
              redraw_line(line);
              break;
          }
        }
        break;

      default:
        line[pos++] = c;
        line[pos] = '\0';

        write(STDOUT_FILENO, &c, 1);
        break;
    }
  }

  done:
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
  return line;
}

#define AOS_TOK_BUFSIZE 64
Token *aos_realloc_tokens(Token *tokens, int *bufsize)
{
  int old_bufsize = *bufsize;
  *bufsize += AOS_TOK_BUFSIZE;
  Token *tmp = realloc(tokens, (*bufsize) * sizeof(Token));
  if (!tmp) {
    fprintf(stderr, "aos: allocation error\n");
    free(tokens);
    exit(EXIT_FAILURE);
  }

  memset(tmp + old_bufsize, 0, (*bufsize - old_bufsize) * sizeof(Token));
  return tmp;
}

void aos_free_tokens(Token *tokens)
{
  if (tokens == NULL) {
    return;
  }

  for (int i = 0; tokens[i].value != NULL; i++) {
    free(tokens[i].value);
  }
  free(tokens);
}

bool alias_active = false;
Token *aos_alias_replace(Token *tokens, int *position, int *bufsize) {
  int pos = *position;

  if (pos <= 0 || tokens[pos - 1].value == NULL) {
    return tokens;
  }

  for (int i = 0; i < MAX_ALIAS; i++) {
    if (aliases[i].name == NULL || aliases[i].value == NULL) {
      continue;
    }

    if (strcmp(tokens[pos - 1].value, aliases[i].name) == 0) {
      alias_active = true;
      Token *alias_tokens = aos_tokenize_line(aliases[i].value);

      for (int j = 0; alias_tokens[j].value != NULL; j++) {
        if (pos >= *bufsize) {
          tokens = aos_realloc_tokens(tokens, bufsize);
        }

        tokens[pos - 1].value = aos_strdup_check(alias_tokens[j].value);
        tokens[pos - 1].quoted = alias_tokens[j].quoted;
        pos++;
      }

      aos_free_tokens(alias_tokens);
      break;
    }
  }

  *position = pos;
  alias_active = false;
  return tokens;
}

#define AOS_OP_DELIM "|<>"
#define AOS_WHITE_DELIM " \t\r\n\a"
Token *aos_tokenize_line(char *line) 
{
  int bufsize = AOS_TOK_BUFSIZE, position = 0;
  int token_size = 64;
  Token *tokens = calloc(bufsize, sizeof(Token));
  char *token = calloc(1, token_size);
  token[0] = '\0';
  int token_pos = 0;
  bool in_quotes = false;
  bool is_operator = false;
  char quote_char = '\0';

  if (!tokens || !token) {
    fprintf(stderr, "aos: allocation error\n");
    free(tokens);
    free(token);
    exit(EXIT_FAILURE);
  }

  // Loop through each character in line
  for (char *str = line; *str != '\0'; str++) {
    // Check if we are currently inside quotes
    if (in_quotes) {
      if (quote_char == *str) {
        quote_char = '\0';
        in_quotes = false;
        if(token_pos > 0) {
          tokens[position].value = aos_strdup_check(token);
          tokens[position].quoted = true;
          token_pos = 0;
          token[0] = '\0';
          position++; 
          // Increase buffer size if necessary
          if (position == bufsize) {
            tokens = aos_realloc_tokens(tokens, &bufsize);
          }
        }
        continue; // skip the quote character
      } else {
        token[token_pos] = *str;
        token[token_pos + 1] = '\0';
        token_pos++;
        if (token_pos + 1 >= token_size) {
          token_size += 64;
          char *tmp = realloc(token, token_size);
          if (!tmp) {
            fprintf(stderr, "aos: allocation error\n");
            free(token);
            exit(EXIT_FAILURE);
           }
          token = tmp;
        }
      }
    } else { // Not in quotes
      if (strchr(AOS_WHITE_DELIM, *str) != NULL) { // Check if the character is a whitespace delimiter
        is_operator = false; // reset operator flag
        if ((token_pos > 0)) {
          // Increase buffer size if necessary
          if (position == bufsize) {
            tokens = aos_realloc_tokens(tokens, &bufsize);
          }
          tokens[position].value = aos_strdup_check(token);
          tokens[position].quoted = false;
          token_pos = 0;
          token[0] = '\0';
          position++;
          // Increase buffer size if necessary
          if (position == bufsize) {
            tokens = aos_realloc_tokens(tokens, &bufsize);
          }
          if ((((position - 1) == 0) || (strchr(tokens[position - 2].value, '|') != NULL)) && (alias_active == false)) {
            tokens = aos_alias_replace(tokens, &position, &bufsize);
          }
        } else { // skip whitespace if token is empty
          continue;
        }
      } else if (strchr(AOS_OP_DELIM, *str) != NULL) { // Check if the character is an operator
        if ((token_pos > 0) && !is_operator) {
          // Increase buffer size if necessary
          if (position == bufsize) {
            tokens = aos_realloc_tokens(tokens, &bufsize);
          }

          tokens[position].value = aos_strdup_check(token);
          tokens[position].quoted = false;
          token_pos = 0;
          token[0] = '\0';
          position++;
          // Increase buffer size if necessary
          if (position == bufsize) {
            tokens = aos_realloc_tokens(tokens, &bufsize);
          }
          if ((((position - 1) == 0) || (strchr(tokens[position - 2].value, '|') != NULL)) && (alias_active == false)) {
            tokens = aos_alias_replace(tokens, &position, &bufsize);
          }
        }
        is_operator = true; // operator token

        // Increase buffer size if necessary
        if (position == bufsize) {
          tokens = aos_realloc_tokens(tokens, &bufsize);
        }

        token[token_pos] = *str;
        token[token_pos + 1] = '\0';
        token_pos++;
        if (token_pos + 1 >= token_size) {
          token_size += 64;
          char *tmp = realloc(token, token_size);
          if (!tmp) {
            fprintf(stderr, "aos: allocation error\n");
            free(token);
            exit(EXIT_FAILURE);
          }
          token = tmp;
        }
      } else if (*str == '\"' || *str == '\'') { // Start of a quoted token
        in_quotes = true; // in quotes
        quote_char = *str;
        continue; // skip the quote character
      } else {
        if (token_pos > 0 && is_operator) {
          // Increase buffer size if necessary
          if (position == bufsize) {
            tokens = aos_realloc_tokens(tokens, &bufsize);
          }

          tokens[position].value = aos_strdup_check(token);
          tokens[position].quoted = false;
          token_pos = 0;
          token[0] = '\0';
          position++;
          // Increase buffer size if necessary
          if (position == bufsize) {
            tokens = aos_realloc_tokens(tokens, &bufsize);
          }
        }
        is_operator = false; // command token
        token[token_pos] = *str;
        token[token_pos + 1] = '\0';
        token_pos++;
        if (token_pos + 1 >= token_size) {
          token_size += 64;
          char *tmp = realloc(token, token_size);
          if (!tmp) {
            fprintf(stderr, "aos: allocation error\n");
            free(token);
            exit(EXIT_FAILURE);
          }
          token = tmp;
        }
      }
    }
  }

  if (in_quotes) {
    fprintf(stderr, "aos: Unterminated quote, must close quote with %c\n", quote_char);
    free(token);
    aos_free_tokens(tokens);
    return NULL;
  } else if ((token_pos > 0)) {
    // Increase buffer size if necessary
    if (position == bufsize) {
      tokens = aos_realloc_tokens(tokens, &bufsize);
    }

    tokens[position].value = aos_strdup_check(token);
    tokens[position].quoted = false;
    token_pos = 0;
    token[0] = '\0';
    position++;
    // Increase buffer size if necessary
    if (position == bufsize) {
      tokens = aos_realloc_tokens(tokens, &bufsize);
    }
    if ((((position - 1) == 0) || (strchr(tokens[position - 2].value, '|') != NULL)) && (alias_active == false)) {
      tokens = aos_alias_replace(tokens, &position, &bufsize);
    }
  }

  free(token);
  return tokens;
}





/*
  Part 3: Shell Command Parsing
*/
struct redirections {
  char *input_file; // <
  char *output_file; // > and >>
  bool append_output; // false = >, true = >>
};

struct command {
  char **argv;
  struct redirections *redir;
};

struct pipeline {
  struct command *commands;
  int num_commands;
  bool background; // true if the pipeline should run in the background
};

void aos_free_pipeline(struct pipeline *p)
{
  if (p == NULL || p->commands == NULL) {
    return;
  }

  for (int i = 0; i < p->num_commands; i++) {
    free(p->commands[i].argv);
    if (p->commands[i].redir != NULL) {
      free(p->commands[i].redir->input_file);
      free(p->commands[i].redir->output_file);
      free(p->commands[i].redir);
    }
  }

  free(p->commands);
  p->commands = NULL;
  p->num_commands = 0;
}

int aos_command_length(Token *tokens) {
  if (tokens == NULL || tokens[0].value == NULL) {
    return 0;
  }

  int num_commands = 1;
  for (int i = 0; tokens[i].value != NULL; i++) {
    if (strcmp(tokens[i].value, "|") == 0) {
      num_commands += 1;
    }
  }
  return num_commands;
}

// Convert array of Tokens to array of char* for execvp
char **aos_get_token_values(Token *tokens)
{
  int count = 0;
  while (tokens[count].value != NULL) {
    count++;
  }

  char **values = calloc(count + 1, sizeof(char *));
  if (!values) {
    fprintf(stderr, "aos: allocation error\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < count; i++) {
    values[i] = tokens[i].value;
  }
  values[count] = NULL;

  return values;
}

size_t get_array_length(char **arr) {
  size_t length = 0;
  // Loop until the pointer itself is NULL
  while (arr[length] != NULL) {
    length++;
  }
  return length;
}

Token *aos_expand_variables (Token *tokens) {
  for (int i = 0; tokens[i].value != NULL; i++) {
    if (tokens[i].value[0] == '$') {
      char *name = tokens[i].value + 1;
      char *value = getenv(name);
      free(tokens[i].value);
      if (value != NULL)
          tokens[i].value = aos_strdup_check(value);
      else
          tokens[i].value = aos_strdup_check("");
    } 
  }
  return tokens;
}

struct pipeline aos_create_pipeline(Token *tokens)
{
  // Set up the pipeline
  struct pipeline pipe;
  pipe.num_commands = aos_command_length(tokens);
  pipe.commands = calloc(pipe.num_commands, sizeof(struct command));
  pipe.background = false;

  if (pipe.num_commands == 0) {
    return pipe;
  }

  // Check for Background Execution
  int token_count = 0;
  while (tokens[token_count].value != NULL) {
    token_count++;
  }
  if (strcmp(tokens[token_count - 1].value, "&") == 0) {
    pipe.background = true;
    tokens[token_count - 1].value = NULL;
  }

  // Section off token values into commands
  char **values = aos_get_token_values(tokens);
  int cmd_index = 0;
  int cmd_count = 1; // Start at 1 to account for the NULL terminator for argv
  int cmd_start = 0; // Track where current command starts in values array

  // Loop through tokens and handle operations
  for (int i = 0; values[i] != NULL; i++) {
    if (strcmp(values[i], "|") == 0) { // Pipe operator
      if (cmd_count == 1) {
        fprintf(stderr, "aos: No command between pipes\n");
        exit(EXIT_FAILURE);
      } else {
        pipe.commands[cmd_index].argv = calloc(cmd_count, sizeof(char *));
        for (int j = 0; j < (cmd_count - 1); j++) {
          pipe.commands[cmd_index].argv[j] = values[cmd_start + j];
        }
        pipe.commands[cmd_index].argv[cmd_count - 1] = NULL;
        cmd_start = i + 1; // Next command starts after the pipe
        cmd_count = 1; // Reset count for next command
        cmd_index++;
      }
    } else if (strcmp(values[i], "<") == 0) { // Input redirection operator
      if (pipe.commands[cmd_index].redir == NULL) {
        pipe.commands[cmd_index].redir = malloc(sizeof(struct redirections));
        pipe.commands[cmd_index].redir->input_file = NULL;
        pipe.commands[cmd_index].redir->output_file = NULL;
        pipe.commands[cmd_index].redir->append_output = false;
      }
      pipe.commands[cmd_index].redir->input_file = aos_strdup_check(values[i + 1]);
      i++; // Skip the next token since it's the file name
    } else if ((strcmp(values[i], ">") == 0) || (strcmp(values[i], ">>") == 0)) { // Output redirection operator (Override)
      if (pipe.commands[cmd_index].redir == NULL) {
        pipe.commands[cmd_index].redir = malloc(sizeof(struct redirections));
        pipe.commands[cmd_index].redir->input_file = NULL;
        pipe.commands[cmd_index].redir->output_file = NULL;
        pipe.commands[cmd_index].redir->append_output = false;
      }
      pipe.commands[cmd_index].redir->output_file = aos_strdup_check(values[i + 1]);
      pipe.commands[cmd_index].redir->append_output = (strcmp(values[i], ">>") == 0);
      i++; // Skip the next token since it's the file name
    } else { // Not an operator
      cmd_count++;
    }
  }

  // Handle the last command (after the final loop iteration)
  if (cmd_count > 1) {
    pipe.commands[cmd_index].argv = calloc(cmd_count, sizeof(char *));
    for (int j = 0; j < (cmd_count - 1); j++) {
      pipe.commands[cmd_index].argv[j] = values[cmd_start + j];
    }
    pipe.commands[cmd_index].argv[cmd_count - 1] = NULL;
  }

  free(values);
  return pipe;
}

struct pipeline aos_parse_tokens(Token *tokens) {
  char **values = aos_get_token_values(tokens);
  struct pipeline pipe = {0};
  pipe.commands = NULL;
  pipe.num_commands = 0;

  // Enviornment Variable
  if ((get_array_length(values) == 1) && (strchr(values[0], '=') != NULL)) {
    // Use a duplicate to avoid modifying the original token with strtok
    char *assignment = aos_strdup_check(values[0]);
    char *env_var_name = strtok(assignment, "=");
    if (env_var_name[0] == '$'){
      printf("aos: invalid assignment\n");
      free(assignment);
      free(values);
      return pipe;
    }
    char *env_var_value = strtok(NULL, "=");
    if (env_var_value == NULL) {
      env_var_value = "";
    }
    printf("Setting environment variable: %s=%s\n", env_var_name, env_var_value);
    setenv(env_var_name, env_var_value, 1);
    free(assignment);
    free(values);
    return pipe; // Return an empty pipeline since this is not a command to execute
  }

  free(values);
  Token *expanded_tokens = aos_expand_variables(tokens);
  pipe = aos_create_pipeline(expanded_tokens);
  return pipe;
}





/*
  Part 4: Command Execution
*/
char *aos_build_command_string (struct pipeline *p) {
  size_t len = 0;
  for (int c = 0; c < p->num_commands; c++) {
    for (int a = 0; p->commands[c].argv[a] != NULL; a++) {
      len += strlen(p->commands[c].argv[a]);
      if (p->commands[c].argv[a + 1] != NULL) {
        len++;
      }    
    }
    if (c < p->num_commands - 1){
      len += 3;  
    }
  }
  len++;

  char *command = malloc(len + 1);
  if (command == NULL) {
    perror("malloc");
    return NULL;
  }
  command[0] = '\0';
  for (int c = 0; c < p->num_commands; c++) {
    for (int a = 0; p->commands[c].argv[a] != NULL; a++) {
      strcat(command, p->commands[c].argv[a]);
      if (p->commands[c].argv[a + 1] != NULL) {
        strcat(command, " ");
      } 
    }
    if (c < p->num_commands - 1) {
      strcat(command, " | ");
    }
  }
  return command;
}

void aos_sigchld_handler(int sig)
{
  (void)sig;
  int status;
  pid_t pid;
  
  // Reap all terminated child processes
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0)
  {
    // Use the mapping to get pgid instead of getpgid() which fails on exited processes
    pid_t pgid = pid_pgid_map_get(pid);
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      jobs_update_state(pgid, JOB_DONE);
      pid_pgid_map_remove(pid);
    } else if (WIFSTOPPED(status)) {
      jobs_update_state(pgid, JOB_STOPPED);
    } else if (WIFCONTINUED(status)) {
      jobs_update_state(pgid, JOB_RUNNING);
    }
  }
}

void aos_run_command(char **argv)
{
  for (int i = 0; i < aos_num_builtins(); i++) {
    if (strcmp(argv[0], builtin_str[i]) == 0) {
      exit((*builtin_func[i])(argv));
    }
  }

  execvp(argv[0], argv);

  perror("aos");
  exit(EXIT_FAILURE);
}

void aos_exec_child(char **argv)
{
  struct sigaction sa_default;
  sigemptyset(&sa_default.sa_mask);
  sa_default.sa_flags = 0;
  sa_default.sa_handler = SIG_DFL;
  sigaction(SIGINT, &sa_default, NULL);
  sigaction(SIGTSTP, &sa_default, NULL);

  aos_run_command(argv);
}

int aos_execute_pipeline(struct pipeline *p)
{
  int num_cmds = p->num_commands;
  pid_t pgid = 0;

  // Empty line
  if (num_cmds == 0) {
    return 1;
  }

  // Single command: run builtins in the shell, otherwise fork and execute in child
  if (num_cmds == 1) {
    char **argv = p->commands[0].argv;

    if (argv == NULL || argv[0] == NULL) {
      return 1;
    }

    for (int i = 0; i < aos_num_builtins(); i++) {
      if (strcmp(argv[0], builtin_str[i]) == 0) {
        return (*builtin_func[i])(argv);
      }
    }

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return 1;
    }

    if (pid > 0) {
      if (setpgid(pid, pid) == -1) {
        perror("setpgid");
      }
      // Map this pid to its pgid for later lookup in SIGCHLD handler
      pid_pgid_map_add(pid, pid);
      if (!p->background) {
        tcsetpgrp(STDIN_FILENO, pid);
      }
    }

    if (pid == 0) {
      if (setpgid(0, 0) == -1) {
        perror("setpgid");
      }
      aos_exec_child(argv);
    }
    
    if (!p->background) {
      int status;
      if (waitpid(pid, &status, WUNTRACED) == -1) {
        perror("waitpid");
        return 1;
      }
      tcsetpgrp(STDIN_FILENO, shell_pgid);
      if (WIFSTOPPED(status)) {
        pgid = getpgid(pid);
        if (jobs_find_by_pgid(pgid) == NULL) {
          char *cmd = aos_build_command_string(p);
          jobs_add(pgid, cmd, false);
          free(cmd);
        }
        jobs_update_state(pgid, JOB_STOPPED);
      }
    } else {
      char *cmd = aos_build_command_string(p);
      pgid = getpgid(pid);
      jobs_add(pgid, cmd, true);
      free(cmd);
    }
    return 1;
  }

  // Create the pipes to connect in/out
  int num_pipes = num_cmds - 1;
  int pipefds[2 * num_pipes];   // pipefds[0] = pipe1 read, pipefds[1] = pipe1 write, pipefds[2] = pipe2 read, pipefds[3] = pipe2 write, ...
  pid_t *pids = malloc(num_cmds * sizeof(pid_t));

  if (!pids) {
    perror("malloc");
    return 1;
  }

  // Make sure all pipes dont return an error
  for (int i = 0; i < num_pipes; i++) {
    if (pipe(pipefds + i * 2) == -1) {
      perror("pipe");

      // close any pipes already opened
      for (int j = 0; j < i; j++) {
        close(pipefds[j * 2]);
        close(pipefds[j * 2 + 1]);
      }

      goto cleanup;
    }
  }

  // Fork one child per command
  for (int i = 0; i < num_cmds; i++) {
    pids[i] = fork();
    if (pids[i] < 0) {
      perror("fork");

      // parent cleanup
      for (int j = 0; j < 2 * num_pipes; j++) {
        close(pipefds[j]);
      }

      goto cleanup;
    }

    if (pids[i] > 0) {
      if (pgid == 0) {
        pgid = pids[i];
      }
        
      if (setpgid(pids[i], pgid) == -1) {
        perror("setpgid");
      }
      // Map this pid to the process group for later lookup in SIGCHLD handler
      pid_pgid_map_add(pids[i], pgid);
    }

    if (pids[i] == 0) {
      if (pgid == 0) {
        pgid = getpid();
      }
      if (setpgid(0, pgid) == -1) {
          perror("setpgid");
          exit(EXIT_FAILURE);
      }

      // If not first command, hook stdin to previous pipe read end
      if (i > 0) {
        int read_fd = pipefds[(i - 1) * 2];
        if (dup2(read_fd, STDIN_FILENO) == -1) {
          perror("dup2 stdin");
          exit(EXIT_FAILURE);
        }
      }

      // If not last command, hook stdout to current pipe write end
      if (i < num_cmds - 1) {
        int write_fd = pipefds[i * 2 + 1];
        if (dup2(write_fd, STDOUT_FILENO) == -1) {
          perror("dup2 stdout");
          exit(EXIT_FAILURE);
        }
      }

      if (p->commands[i].redir != NULL) {
        if (p->commands[i].redir->input_file != NULL) {
          int input_fd = open(p->commands[i].redir->input_file, O_RDONLY);
          if (input_fd == -1) {
            perror("open input redirection");
            exit(EXIT_FAILURE);
          }
          if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("dup2 input redirection");
            close(input_fd);
            exit(EXIT_FAILURE);
          }
          close(input_fd);
        }

        if (p->commands[i].redir->output_file != NULL) {
          int flags = O_WRONLY | O_CREAT | O_TRUNC;
          if (p->commands[i].redir->append_output) {
            flags = O_WRONLY | O_CREAT | O_APPEND;
          }

          int output_fd = open(p->commands[i].redir->output_file, flags, 0644);
          if (output_fd == -1) {
            perror("open output redirection");
            exit(EXIT_FAILURE);
          }
          if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2 output redirection");
            close(output_fd);
            exit(EXIT_FAILURE);
          }
          close(output_fd);
        }
      }

      // after dup2, close every pipe FD in the child
      for (int j = 0; j < 2 * num_pipes; j++) {
        close(pipefds[j]);
      }

      aos_exec_child(p->commands[i].argv);
    }
  }

  if (!p->background && (pgid != 0)) {
    if (tcsetpgrp(STDIN_FILENO, pgid) == -1) {
      perror("tcsetpgrp");
    }
  }

  // Parent closes all pipe FDs
  for (int i = 0; i < 2 * num_pipes; i++) {
    close(pipefds[i]);
  }

  // Parent waits for all children
  int status;
  if (!p->background) {
    while (waitpid(-pgid, &status, WUNTRACED) > 0) {
      if (WIFSTOPPED(status)) {
        if (jobs_find_by_pgid(pgid) == NULL) {
          char *cmd = aos_build_command_string(p);
          jobs_add(pgid, cmd, false);
          free(cmd);
        }
        jobs_update_state(pgid, JOB_STOPPED);
        break;
      }
    }
    
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
      perror("tcsetpgrp");
    }
  } else {
    char *cmd = aos_build_command_string(p);
    jobs_add(pgid, cmd, true);
    free(cmd);
  }

  cleanup:
  free(pids);
  return 1;
}





/*
  Part 5: Main loop
*/
static void aos_add_history(const char *command)
{
  for (int i = 0; i < MAX_HISTORY; i++) {
    if (history_list[i] == NULL) {
      history_list[i] = aos_strdup_check(command);
      return;
    }
  }

  free(history_list[0]);
  for (int i = 1; i < MAX_HISTORY; i++) {
    history_list[i - 1] = history_list[i];
  }
  history_list[MAX_HISTORY - 1] = aos_strdup_check(command);
}

void aos_loop(void)
{
  char *line;
  Token *tokens;
  struct pipeline pipe;
  int status;
  char *cwd;
  int history_index = 0;

  do {
    // Command Loop
    cwd = getcwd(NULL, 0);
    printf("aos %s> ", cwd);
    fflush(stdout);
    free(cwd);
    line = aos_read_line(history_index);
    tokens = aos_tokenize_line(line);

    if (tokens == NULL || tokens[0].value == NULL) {
      aos_free_tokens(tokens);
      free(line);
      continue;
    }
    line[strcspn(line, "\n")] = '\0';
    aos_add_history(line);
    history_index++;

    pipe = aos_parse_tokens(tokens);
    status = aos_execute_pipeline(&pipe);

    // Free Memory
    aos_free_pipeline(&pipe);
    free(line);
    aos_free_tokens(tokens);
  } while (status);
}

int main(int argc, char **argv)
{
  // Load config files
  jobs_init();

  // Set up signal handling
  shell_pgid = getpid();
  if (setpgid(shell_pgid, shell_pgid) == -1) {
    perror("setpgid");
  }
  if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
    perror("tcsetpgrp");
  }

  struct sigaction sa_default;
  sigemptyset(&sa_default.sa_mask);
  sa_default.sa_flags = SA_RESTART;
  sa_default.sa_handler = SIG_IGN;
  sigaction(SIGINT,  &sa_default, NULL);
  sigaction(SIGTSTP, &sa_default, NULL);
  sigaction(SIGTTIN, &sa_default, NULL);
  sigaction(SIGTTOU, &sa_default, NULL);

  struct sigaction sa_chld;
  sigemptyset(&sa_chld.sa_mask);
  sa_chld.sa_flags = SA_RESTART;
  sa_chld.sa_handler = aos_sigchld_handler;
  sigaction(SIGCHLD, &sa_chld, NULL);

  // Run command loop.
  aos_loop();

  // Perform any shutdown/cleanup.
  return EXIT_SUCCESS;
}