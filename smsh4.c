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
    // read the next command line from the user

    if ((arglist = splitline(cmdline)) != NULL) {
      // split the command line into arguments

      glob_args(arglist);
      // perform globbing on the arguments

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
        char **command = NULL, **file = NULL;
        int redirect_type = -1;
        split_redirection(arglist, &command, &file, &redirect_type);
        // check if the command contains input/output redirection and split it

        if (redirect_type != -1) {
          result = execute_with_redirection(command, file, redirect_type);
          // execute the command with the specified input/output redirection
        } else {
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
        // child process connect stdout to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        execvp(cmd1[0], cmd1);
        perror("execute command 1");
        return -1;
      default:
        // connect stdin to the read end of the pipe
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
        // found a pipe symbol, split the command into two parts
        arglist[i] = NULL;
        pipe_cmds[0] = arglist;
        pipe_cmds[1] = &arglist[i + 1];
        return 1;  // indicate that a pipe symbol was found and split was
                   // performed
      }
      i++;
    }
    // no pipe symbol found, return the entire command as a single part
    pipe_cmds[0] = arglist;
    return 0;  // indicate that no pipe symbol was found
  }

  int split_redirection(char **arglist, char ***command, char ***file,
                        int *redirect_type) {
    for (int i = 0; arglist[i] != NULL; i++) {
      if (strcmp(arglist[i], "<") == 0 || strcmp(arglist[i], ">") == 0) {
        *redirect_type = (strcmp(arglist[i], "<") == 0) ? 0 : 1;
        arglist[i] = NULL;  // cut arglist at the redirection operator
        *command = arglist;
        *file = &arglist[i + 1];  // file is the part of arglist after the
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
      if (redirect_type == 0) {  // input redirection
        fd = open(*file, O_RDONLY);
        if (fd == -1) {
          perror("open");
          return 1;
        }
        dup2(fd, STDIN_FILENO);
      } else if (redirect_type == 1) {  // output redirection
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
            strdup(globbuf.gl_pathv[0]);  // replace arg with first glob match
      }

      globfree(&globbuf);  // free the memory allocated by glob()
    }

    return 0;
  }
