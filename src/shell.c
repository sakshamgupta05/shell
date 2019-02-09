#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_CMD_LIMIT 200
#define NUM_ARG_LIMIT 10

int main(int argc, char* argv[]) {
  while (1) {
    int blocking = 1;

    char cmd[MAX_CMD_LIMIT]; 
    fgets(cmd, MAX_CMD_LIMIT, stdin); 
    char* args[NUM_ARG_LIMIT + 1];
    char* cmdTrm = strtok(cmd, "\n");
    if (cmdTrm[strlen(cmdTrm) - 1] == '&') {
      blocking = 0;
    }
    char* arg = strtok(cmdTrm, " &");
    int i = 0;
    do {
      args[i++] = arg;
      arg = strtok(NULL, " &");
    } while (arg != NULL && i < NUM_ARG_LIMIT);
    args[i] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork failed");
      exit(1);
    } else if (pid == 0) {
      if (execv(args[0], args) < 0) {
        perror("execv error");
        exit(1);
      }
    }

    int status = 0;
    if (blocking) {
      pid_t childpid = waitpid(pid, &status, 0);
      int retVal = WEXITSTATUS(status);
      if (childpid >= 0) {
        printf("pid: %d, status: %d\n", childpid, retVal);
      }
    } else {
      waitpid(pid, &status, WNOHANG);
    }
  }
  return 0;
}
