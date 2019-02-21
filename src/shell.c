#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <linux/limits.h>
#include <signal.h>
#include <string.h>

#define SC_MAX 1024

char sc[SC_MAX][ARG_MAX];

char **env_path;

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

void exec_cmd(char *cmd) {
  printf("[LOG] %s\n", cmd);
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

void edit_sc_cmd(char *cmd) {
  cmd += 3;
  if (strncmp(cmd, "-i ", 3) == 0) {
    // create entry
    cmd += 3;
    char tmp[ARG_MAX];
    int ind;
    sscanf(cmd, "%d %[^\n]", &ind, tmp);
    strcpy(sc[ind], tmp);
    printf("Command stored successfully\n");
  } else if (strncmp(cmd, "-d ", 3) == 0) {
    // delete entry
    cmd += 3;
    int ind;
    sscanf(cmd, "%d", &ind);
    strcpy(sc[ind], "");
    printf("Command deleted successfully\n");
  } else {
    printf("Incorrect flag for 'sc' command\n");
  }

  printf("$ ");
}

void sigHandler(int sig) {
  printf("\nsc number: ");
  int ind;
  scanf("%d", &ind);
  printf("\npressed\n");
  exec_cmd(sc[ind]);
}

int main(int argc, char* argv[]) {
  if (signal(SIGINT, sigHandler) == SIG_ERR) {
    perror("signal");
  }
  env_path = get_path();

  char cmd[ARG_MAX]; 
  printf("Enter lines of text, ^D to quit:\n");

  printf("$ ");
  while (fgets(cmd, ARG_MAX, stdin) != NULL) {
    cmd[strcspn(cmd, "\n")] = 0;  // remove trailing newline char

    if (strncmp(cmd, "sc ", 3) == 0) {
      edit_sc_cmd(cmd);
    } else {
      exec_cmd(cmd);
    }
  }

  free_path(env_path);
  return 0;
}
