#include <errno.h>
#include <limits.h>
#include <msgs.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_BUFFER_SIZE 512
#define TOKENS_BUFFER_SIZE 512

char currentPath[512];
char prevPath[512];
bool firstRun = true;

void write_withErrorHandling(int stream, const char *text, size_t text_bytes) {
  if (write(stream, text, text_bytes) == -1) {
    if (errno == EINTR)
      write(stream, text,
            text_bytes); // try writing again if interrupted by SIGINT
  }
}
void exit_helpMessage() {
  const char *msg = FORMAT_MSG("exit", EXIT_HELP_MSG);
  write_withErrorHandling(STDOUT_FILENO, msg, strlen(msg));
}

void pwd_helpMessage() {
  const char *msg = FORMAT_MSG("pwd", PWD_HELP_MSG);
  write_withErrorHandling(STDOUT_FILENO, msg, strlen(msg));
}

void cd_helpMessage() {
  const char *msg = FORMAT_MSG("cd", CD_HELP_MSG);
  write_withErrorHandling(STDOUT_FILENO, msg, strlen(msg));
}

void help_helpMessage() {
  const char *msg = FORMAT_MSG("help", HELP_HELP_MSG);
  write_withErrorHandling(STDOUT_FILENO, msg, strlen(msg));
}

void history_helpMessage() {
  const char *msg = FORMAT_MSG("history", HISTORY_HELP_MSG);
  write_withErrorHandling(STDOUT_FILENO, msg, strlen(msg));
}

void displayAllInternalHelpMessages() {
  exit_helpMessage();
  pwd_helpMessage();
  cd_helpMessage();
  history_helpMessage();
  help_helpMessage();
}

void cleanUpZombieProcesses() {
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    // Clean up
  }
}

void displayPrompt() {
  static char pathWithDollar[512];
  strcpy(pathWithDollar, currentPath);
  strcat(pathWithDollar, "$ ");
  write_withErrorHandling(STDOUT_FILENO, pathWithDollar,
                          strlen(pathWithDollar));
}

int parse_commandNum_string(const char *str) {
  if (!str || str[0] != '!')
    return -1;

  unsigned int number = 0;
  str++;

  while (*str) {
    if (*str < '0' || *str > '9')
      return -1;

    if (number > (UINT_MAX - (*str - '0')) / 10)
      return -1;

    number = number * 10 + (*str - '0');
    str++;
  }
  return (int)number;
}

void uint_to_string(unsigned int value, char *buffer) {
  const size_t max_digits = 10;
  char temp[max_digits];
  size_t num_digits = 0;

  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return;
  }

  while (value != 0) {
    temp[num_digits++] = (value % 10) + '0';
    value /= 10;
  }

  for (size_t i = 0; i < num_digits; ++i) {
    buffer[i] = temp[num_digits - 1 - i];
  }

  buffer[num_digits] = '\0';
}

void sigint_handler(int sig) {
  write(STDOUT_FILENO, "\n", sizeof("\n"));
  displayAllInternalHelpMessages();
  displayPrompt();
}

bool isHistoryCommand(const char *buffer) {
  return buffer[0] == '!';
}

int main() {
  char buffer[INPUT_BUFFER_SIZE];
  char *saveptr;

  // History feature stuff
  unsigned int commandsEntered = 0;
  char commands[10][INPUT_BUFFER_SIZE];

  // SIGINT handler setup
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = &sigint_handler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, NULL);

  while (1) {
    // Clean up zombie processes
    cleanUpZombieProcesses();

    // Clear buffer
    memset(buffer, 0, sizeof(buffer));

    // Save current path (and prev path as well if first run)
    getcwd(currentPath, sizeof(currentPath));
    if (firstRun) {
      strcpy(prevPath, currentPath);
      firstRun = false;
    }

    displayPrompt();

    ssize_t bytes_entered = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (bytes_entered > 1 || (bytes_entered == 1 && buffer[0] != '\n')) {

      // Null terminate
      buffer[bytes_entered] = '\0';
      char *newline = strchr(buffer, '\n');
      if (newline != NULL) {
        *newline = '\0';
      }

    beginExecution:;
      // Add current command to stack
      if (!isHistoryCommand(buffer)) {
        commandsEntered += 1;
        for (unsigned int i = 9; i >= 1; --i) {
          strcpy(commands[i], commands[i - 1]);
        }
        strcpy(commands[0], buffer);
      }

      // Tokenize
      char *tokens[TOKENS_BUFFER_SIZE];
      unsigned int numTokens = 0;
      const char *delim = " ";
      char *currToken = strtok_r(buffer, delim, &saveptr);
      while (currToken != NULL) {
        tokens[numTokens] = currToken;
        numTokens++;
        currToken = strtok_r(NULL, delim, &saveptr);
      }
      tokens[numTokens] = NULL;

      // Internal commands
      if (strcmp(tokens[0], "exit") == 0) {
        if (numTokens > 1) {
          const char *err = FORMAT_MSG("exit", TMA_MSG);
          write_withErrorHandling(STDERR_FILENO, err, strlen(err));
        } else {
          _exit(0);
        }
      } else if (strcmp(tokens[0], "pwd") == 0) {
        if (numTokens > 1) {
          const char *err = FORMAT_MSG("pwd", TMA_MSG);
          write_withErrorHandling(STDERR_FILENO, err, strlen(err));
        } else {
          char cwd[512];
          if (getcwd(cwd, sizeof(cwd)) != NULL) {
            strcat(cwd, "\n");
            write_withErrorHandling(STDOUT_FILENO, cwd, strlen(cwd));
          } else {
            const char *err = FORMAT_MSG("pwd", GETCWD_ERROR_MSG);
            write_withErrorHandling(STDERR_FILENO, err, strlen(err));
          }
        }
      } else if (strcmp(tokens[0], "cd") == 0) {
        if (numTokens > 2) {
          const char *err = FORMAT_MSG("cd", TMA_MSG);
          write_withErrorHandling(STDERR_FILENO, err, strlen(err));
        } else {
          // Save current directory
          getcwd(currentPath, sizeof(currentPath));

          // Get current user's username
          struct passwd *pw;
          uid_t uid;
          uid = getuid();
          pw = getpwuid(uid);

          // Concatenate username to make string of format "/home/[USERNAME]"
          char path[96];
          strcpy(path, "/home/");
          path[sizeof(path) - 1] = '\0';
          strcat(path, pw->pw_name);

          int chdir_return;
          if (numTokens == 1 || strcmp(tokens[1], "~") == 0) {
            chdir_return = chdir(path);
          } else if (tokens[1][0] == '~' && tokens[1][1] == '/') {
            char temp[512];
            strcpy(temp, path);
            strcat(temp, tokens[1] + 1);
            chdir_return = chdir(temp);
          } else if (strcmp(tokens[1], "-") == 0) {
            chdir_return = chdir(prevPath);
          } else {
            chdir_return = chdir(tokens[1]);
          }
          if (chdir_return == -1) {
            const char *err = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
            write_withErrorHandling(STDERR_FILENO, err, strlen(err));
          } else {
            strcpy(prevPath, currentPath);
          }
        }
      } else if (strcmp(tokens[0], "help") == 0) {
        if (numTokens > 2) {
          const char *err = FORMAT_MSG("help", TMA_MSG);
          write_withErrorHandling(STDERR_FILENO, err, strlen(err));
        } else if (numTokens == 1) {
          displayAllInternalHelpMessages();
        } else {
          static const char *internalCommands[] = {"exit", "pwd", "cd", "help"};
          bool isInternalCommand = false;
          for (size_t i = 0; i < 4; ++i) {
            if (strcmp(tokens[1], internalCommands[i]) == 0)
              isInternalCommand = true;
          }
          if (isInternalCommand) {
            if (strcmp(tokens[1], "exit") == 0) {
              exit_helpMessage();
            } else if (strcmp(tokens[1], "pwd") == 0) {
              pwd_helpMessage();
            } else if (strcmp(tokens[1], "cd") == 0) {
              cd_helpMessage();
            } else if (strcmp(tokens[1], "history") == 0) {
              history_helpMessage();
            } else {
              help_helpMessage();
            }
          } else {
            char msg[96];
            strcpy(msg, tokens[1]);
            strcat(msg, ": ");
            strcat(msg, EXTERN_HELP_MSG);
            strcat(msg, "\n");
            write_withErrorHandling(STDOUT_FILENO, msg, strlen(msg));
          }
        }
      } else if (strcmp(tokens[0], "history") == 0) {
        char line[512];
        unsigned int cap = commandsEntered < 10 ? commandsEntered : 10;
        for (unsigned int i = 0; i < cap; ++i) {
          char numberString[12];
          uint_to_string(commandsEntered - i - 1, numberString);
          memset(line, 0, 512);
          strcat(line, numberString);
          strcat(line, "\t");
          strcat(line, commands[i]);
          strcat(line, "\n");
          write_withErrorHandling(STDOUT_FILENO, line, strlen(line));
        }
      } else if (numTokens == 1 &&
                 (tokens[0][0] == '!' && tokens[0][1] != '!')) {
        int commandNum = parse_commandNum_string(tokens[0]);
        int lowerBound = commandsEntered - 1 - 10;
        int upperBound = commandsEntered - 1;
        bool withinBounds = false;
        if (commandNum >= lowerBound && commandNum <= upperBound)
          withinBounds = true;

        if (commandNum == -1 || !withinBounds) {
          const char *err = FORMAT_MSG("history", HISTORY_INVALID_MSG);
          write_withErrorHandling(STDERR_FILENO, err, strlen(err));
        } else {
          unsigned int commandToRun = commandsEntered - 1 - commandNum;
          strcpy(buffer, commands[commandToRun]);
          char temp[512];
          strcpy(temp, commands[commandToRun]);
          strcat(temp, "\n");
          write_withErrorHandling(STDOUT_FILENO, temp, strlen(temp));
          goto beginExecution;
        }
      } else if (numTokens == 1 && (strcmp(tokens[0], "!!") == 0)) {
        if (commandsEntered == 0) {
          const char *err = FORMAT_MSG("history", HISTORY_NO_LAST_MSG);
          write_withErrorHandling(STDERR_FILENO, err, strlen(err));
        } else {
          strcpy(buffer, commands[0]);
          char temp[512];
          strcpy(temp, commands[0]);
          strcat(temp, "\n");
          write_withErrorHandling(STDOUT_FILENO, temp, strlen(temp));
          goto beginExecution;
        }
        // Non-internal commands
      } else {
        // Check if command should be executed in background
        bool background = false;
        if (strcmp(tokens[numTokens - 1], "&") == 0) {
          background = true;
          tokens[numTokens - 1] = NULL;
        }

        // Launch command in child process
        pid_t pid = fork();

        if (pid == 0) {
          // Child
          if (execvp(tokens[0], tokens) == -1) {
            const char *err = FORMAT_MSG("shell", EXEC_ERROR_MSG);
            write_withErrorHandling(STDERR_FILENO, err, strlen(err));
            _exit(EXIT_FAILURE);
          }
        } else if (pid > 0) {
          // Parent
          if (background == false) {
            int status;
          wait_for_child_process:
            if (waitpid(pid, &status, 0) == -1) {
              if (errno == EINTR) {
                goto wait_for_child_process;
              } else {
                const char *err = FORMAT_MSG("shell", WAIT_ERROR_MSG);
                write_withErrorHandling(STDERR_FILENO, err, strlen(err));
              }
            }
          }
        } else {
          // Fork failed
          const char *err = FORMAT_MSG("shell", FORK_ERROR_MSG);
          write_withErrorHandling(STDERR_FILENO, err, strlen(err));
        }
      }

    } else if ((bytes_entered == 1 && buffer[0] == '\n') ||
               bytes_entered == 0) {
      continue;
    } else {
      if (errno == EINTR) {
        continue;
      } else {
        const char *err = FORMAT_MSG("shell", READ_ERROR_MSG);
        write_withErrorHandling(STDERR_FILENO, err, strlen(err));
      }
    }
  }

  return 0;
}
