#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <linux/limits.h>
#include <string.h>

char** get_path() {
  const char *orig_path = getenv("PATH");
  if (orig_path == NULL) return NULL;
  int path_len = strlen(orig_path);
  char *dup_path = malloc(sizeof(char) * (path_len + 1));
  strcpy(dup_path, orig_path);

  int num_path = 1;
  for (int i = 0; i < path_len; i++) {
    if (dup_path[i] == ':') num_path++;
  }

  char **path = malloc(sizeof(char*) * (num_path + 1));
  char **pp = path;
  *pp = strtok(dup_path, ":");
  while(*pp != NULL) {
    pp++;
    *pp = strtok(NULL, ":");
  }
  return path;
}

void free_path(char **path) {
  free(*path);
  free(path);
}

int main(int argc, char* argv[]) {
  char cmd[ARG_MAX]; 
  printf("Enter lines of text, ^D to quit:\n");

  char **env_path = get_path();

  printf("$ ");
  while (fgets(cmd, ARG_MAX, stdin) != NULL) {
    cmd[strcspn(cmd, "\n")] = 0;  // remove trailing newline char

    int blocking = 1;

    int c_len = strlen(cmd);
    int c_argc = 1;
    for (int i = 0; i < c_len; i++) {
      // TODO: add support for 'cmd "a b"'
      if (cmd[i] == ' ') c_argc++;
    }
    char* c_argv[c_argc + 1];
    if (cmd[c_len - 1] == '&') {
      blocking = 0;
      cmd[c_len - 1] = '\0';
      c_len--;
    }

    char **cp = c_argv;
    *cp = strtok(cmd, " ");
    while(*cp != NULL) {
      cp++;
      *cp = strtok(NULL, " ");
    }

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork failed");
      exit(1);
    } else if (pid == 0) {
      // exec in process dir
      if (execv(c_argv[0], c_argv) < 0) {
        if (errno == ENOENT) {
          // search & exec in PATH
          for (char **pp = env_path; *pp != NULL; pp++) {
            char path[PATH_MAX];
            sprintf(path, "%s/%s", *pp, c_argv[0]);
            if (execv(path, c_argv) < 0) {
              if (errno != ENOENT) {
                perror("execv error");
                exit(1);
              }
            }
          }
        }
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
    printf("$ ");
  }

  free_path(env_path);
  return 0;
}
