#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smsh.h"

#define DFL_PROMPT "> "

int main() {
  char *cmdline, *prompt, **arglist;
  int result;
  void setup();

  prompt = DFL_PROMPT;
  setup();

  while ((cmdline = next_cmd(prompt, stdin)) != NULL) {
    if ((arglist = splitline(cmdline)) != NULL) {
      // check for pipe symbol in arglist and split command if necessary
      char **pipe_cmds[2] = {NULL, NULL};
      int has_pipe = split_pipe(arglist, pipe_cmds);

      if (has_pipe) {
        // fork, create a pipe, and run execute on both sides of the pipe
        int pipefd[2];
        if (pipe(pipefd) == -1) {
          perror("pipe");
          return 1;
        }

        result = execute_with_pipe(pipe_cmds[0], pipe_cmds[1], pipefd);
      } else {
        result = execute(arglist);
      }
      freelist(arglist);
    }
    free(cmdline);
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
      // execute cmd1 and connect stdout to pipefd[1]
      dup2(pipefd[1],
           STDOUT_FILENO);    // redirect stdout to the writing end of the pipe
      close(pipefd[0]);       // close the reading end of the pipe
      execvp(cmd1[0], cmd1);  // execute cmd1
      perror("execute command 1");  // print error if execvp fails
      return -1;
    default:
      // execute cmd2 and connect stdin to pipefd[0]
      dup2(pipefd[0],
           STDIN_FILENO);     // redirect stdin to the reading end of the pipe
      close(pipefd[1]);       // close the writing end of the pipe
      execvp(cmd2[0], cmd2);  // execute cmd2
      perror("execute command 2");  // print error if execvp fails
      return -1;
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
    // no pipe symbol found return the entire command as a single part
    pipe_cmds[0] = arglist;
    return 0;  // indicate that no pipe symbol was found
  }
}
