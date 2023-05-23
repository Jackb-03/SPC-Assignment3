#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "smsh.h"

#define DFL_PROMPT "> "

void setup();
void fatal(char *s1, char *s2, int n);
int split_pipe(char **arglist, char ***pipe_cmds);
int execute_with_pipe(char **cmd1, char **cmd2, int pipefd[2]);
int split_redirection(char **arglist, char ***command, char ***file,
                      int *redirect_type);
int execute_with_redirection(char **command, char **file, int redirect_type);

int main() {
  char *cmdline, *prompt, **arglist;
  int result;

  prompt = DFL_PROMPT;
  setup();

  while ((cmdline = next_cmd(prompt, stdin)) != NULL) {
    // read the next command line from the user

    if ((arglist = splitline(cmdline)) != NULL) {
      // split the command line into arguments

      char **pipe_cmds[2] = {NULL, NULL};
      int has_pipe = split_pipe(arglist, pipe_cmds);
      // check if the command contains a pipe symbol and split the command if
      // necessary

      if (has_pipe) {
        // command contains a pipe symbol
        int pipefd[2];
        pipe(pipefd);
        // create a pipe for communication between commands

        result = execute_with_pipe(pipe_cmds[0], pipe_cmds[1], pipefd);
        // execute the commands with the pipe connected
      } else {
        // command does not contain a pipe symbol
        char **command = NULL, **file = NULL;
        int redirect_type = -1;
        split_redirection(arglist, &command, &file, &redirect_type);
        // check if the command contains input/output redirection and split it
        // if necessary

        if (redirect_type != -1) {
          // command contains input/output redirection
          result = execute_with_redirection(command, file, redirect_type);
          // execute the command with the specified input/output redirection
        } else {
          // regular command execution
          result = execute(arglist);
          // execute the command normally
        }
      }

      freelist(arglist);
      // free the memory allocated for the argument list
    }

    free(cmdline);
    // free the memory allocated for the command line
  }

  return 0;
}

void setup() {
  // interupt and quit now is ignored
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
}

void fatal(char *s1, char *s2, int n) {
  // used for reporting errors and terminating the program
  fprintf(stderr, "Error: %s,%s\n", s1, s2);
  exit(n);
}

int execute_with_pipe(char **cmd1, char **cmd2, int pipefd[2]) {
  int pid;
  switch (pid = fork()) {
    case -1:
      perror("fork");
      return -1;
    case 0:
      // child process execute cmd1 and connect stdout to pipefd[1]
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[0]);
      execvp(cmd1[0], cmd1);
      perror("execute command 1");
      return -1;
    default:
      // execute cmd2 and connect stdin to pipefd[0]
      dup2(pipefd[0], STDIN_FILENO);
      close(pipefd[1]);
      execvp(cmd2[0], cmd2);
      perror("execute command 2");
      return -1;
  }
}

int split_pipe(char **arglist, char ***pipe_cmds) {
  int i = 0;
  while (arglist[i] != NULL) {
    if (strcmp(arglist[i], "|") == 0) {
      arglist[i] = NULL;
      pipe_cmds[0] = arglist;
      pipe_cmds[1] = &arglist[i + 1];
      return 1;
    }
    i++;
  }
  pipe_cmds[0] = arglist;
  return 0;
}

int split_pipe(char **arglist, char ***pipe_cmds) {
  int i = 0;
  while (arglist[i] != NULL) {
    if (strcmp(arglist[i], "|") == 0) {
      // found a pipe symbol split the command into two parts
      arglist[i] = NULL;  // set the pipe symbol position to NULL
      pipe_cmds[0] =
          arglist;  // first part of the command is before the pipe symbol
      pipe_cmds[1] =
          &arglist[i +
                   1];  // second part of the command is after the pipe symbol
      return 1;         // indicate that a pipe symbol was found and split was
                        // performed
    }
    i++;
  }
  // no pipe symbol found, return the entire command as a single part
  pipe_cmds[0] = arglist;
  return 0;  // indicate that no pipe symbol was found
}

int execute_with_redirection(char **command, char **file, int redirect_type) {
  int fd;
  int pid = fork();

  if (pid == 0) {
    // child process
    if (redirect_type == 0) {
      // input redirection
      fd = open(*file, O_RDONLY);
      if (fd == -1) {
        perror("open");
        return 1;
      }
      dup2(fd, STDIN_FILENO);
    } else if (redirect_type == 1) {
      // output redirection
      fd = open(*file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
      if (fd == -1) {
        perror("open");
        return 1;
      }
      dup2(fd, STDOUT_FILENO);
    }

    close(fd);
    execvp(command[0], command);
    perror("execvp");
    return -1;
  } else if (pid > 0) {
    // parent process
    waitpid(pid, NULL, 0);
  } else {
    // fork failed
    perror("fork");
    return -1;
  }
  return 0;
}
