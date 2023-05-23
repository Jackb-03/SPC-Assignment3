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
      // Check for pipe symbol in arglist and split command if necessary
      char **pipe_cmds[2] = {NULL, NULL};
      int has_pipe = split_pipe(arglist, pipe_cmds);

      if (has_pipe) {
        // We'll need to fork, create a pipe, and run execute on both sides of
        // the pipe.
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
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
}

void fatal(char *s1, char *s2, int n) {
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
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[0]);
      execvp(cmd1[0], cmd1);
      perror("execute command 1");
      return -1;
    default:
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
