#include <fcntl.h>
#include <glob.h>
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
int glob_args(char **arglist);

int main() {
  char *cmdline, *prompt, **arglist;
  int result;

  prompt = DFL_PROMPT;
  setup();

  while ((cmdline = next_cmd(prompt, stdin)) != NULL) {
    if ((arglist = splitline(cmdline)) != NULL) {
      glob_args(arglist);  // Perform globbing

      char **pipe_cmds[2] = {NULL, NULL};
      int has_pipe = split_pipe(arglist, pipe_cmds);

      if (has_pipe) {
        int pipefd[2];
        pipe(pipefd);
        result = execute_with_pipe(pipe_cmds[0], pipe_cmds[1], pipefd);
      } else {
        char **command = NULL, **file = NULL;
        int redirect_type = -1;
        split_redirection(arglist, &command, &file, &redirect_type);
        if (redirect_type != -1) {
          result = execute_with_redirection(command, file, redirect_type);
        } else {
          result = execute(arglist);
        }
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

int split_redirection(char **arglist, char ***command, char ***file,
                      int *redirect_type) {
  for (int i = 0; arglist[i] != NULL; i++) {
    if (strcmp(arglist[i], "<") == 0 || strcmp(arglist[i], ">") == 0) {
      *redirect_type = (strcmp(arglist[i], "<") == 0) ? 0 : 1;
      arglist[i] = NULL;  // Cut arglist at the redirection operator
      *command = arglist;
      *file = &arglist[i + 1];  // File is the part of arglist after the
                                // redirection operator
      return 1;
    }
  }
  *command = arglist;
  return 0;
}

int execute_with_redirection(char **command, char **file, int redirect_type) {
  int fd;
  int pid = fork();

  if (pid == 0) {
    if (redirect_type == 0) {  // Input redirection
      fd = open(*file, O_RDONLY);
      if (fd == -1) {
        perror("open");
        return 1;
      }
      dup2(fd, STDIN_FILENO);
    } else if (redirect_type == 1) {  // Output redirection
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
    waitpid(pid, NULL, 0);
  } else {
    perror("fork");
    return -1;
  }
  return 0;
}

int glob_args(char **arglist) {
  glob_t globbuf;

  for (int i = 0; arglist[i] != NULL; i++) {
    if (glob(arglist[i], 0, NULL, &globbuf) == 0) {
      arglist[i] =
          strdup(globbuf.gl_pathv[0]);  // Replace arg with first glob match
    }

    globfree(&globbuf);  // Free the memory allocated by glob()
  }

  return 0;
}
