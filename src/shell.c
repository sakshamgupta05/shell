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

void exec_cmd(char *cmd_str) {
  int cs_len = strlen(cmd_str);

  // set fg if process group
  int fg = 1;
  if (cmd_str[cs_len - 1] == '&') {
    fg = 0;
    cmd_str[cs_len - 1] = '\0';
    cs_len--;
  }

  // split commands (pipes)
  int num_cmds = 1;
  for (int i = 0; i < cs_len; i++) {
    if (cmd_str[i] == '|') {
      num_cmds++;
    }
  }
  char *cmds[num_cmds];
  int ind = 0;
  cmds[ind++] = cmd_str;
  for (int i = 0; i < cs_len; i++) {
    if (cmd_str[i] == '|') {
      cmd_str[i] = '\0';
      for (int j = i-1; cmd_str[j] == ' '; j--) cmd_str[j] = '\0';
      while (cmd_str[i + 1] == ' ') i++;
      cmds[ind++] = cmd_str + i + 1;
    }
  }

  // init pipes
  int p[num_cmds - 1][2];
  for (int i = 0; i < num_cmds - 1; i++) pipe(p[i]);

  pid_t pipelinePgid = 0;
  pid_t lastPid = 0;

  for (int i = 0; i < num_cmds; i++) {
    char *cmd = cmds[i];
    int c_len = strlen(cmd);

    int redir = -1;
    char *fname;
    if (i == 0) {
      for (int i = 0; i < c_len; i++) {
        if (cmd[i] == '<') {
          redir = 0;
          for (int j = i-1; cmd[j] == ' '; j--) cmd[j] = '\0';
          while (cmd[i + 1] == ' ') i++;
          fname = cmd + i + 1;
          cmd[i] = '\0';
          c_len = strlen(cmd);
        }
      }
    }
    if (i == num_cmds - 1) {
      for (int i = 0; i < c_len; i++) {
        if (cmd[i] == '>') {
          cmd[i] = '\0';
          if (cmd[i+1] == '>') {
            redir = 2;
            for (int j = i-1; cmd[j] == ' '; j--) cmd[j] = '\0';
            i++;
            while (cmd[i + 1] == ' ') i++;
            fname = cmd + i + 1;
          } else {
            redir = 1;
            for (int j = i-1; cmd[j] == ' '; j--) cmd[j] = '\0';
            while (cmd[i + 1] == ' ') i++;
            fname = cmd + i + 1;
          }
          c_len = strlen(cmd);
        }
      }
    }

    int c_argc = 1;
    for (int i = 0; i < c_len; i++) {
      if (cmd[i] == ' ') c_argc++;
    }
    char* c_argv[c_argc + 1];

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
      printf("pid: %d\n", getpid());
      signal(SIGTTOU, SIG_DFL);
      if (i == 0) {
        pipelinePgid = getpid();
      }
      // set process gid
      if (setpgid(pipelinePgid, pipelinePgid) < 0) {
        perror("setpgid");
      }
      if (i == 0) {
        // set foreground group
        if (fg) {
          if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
            perror("tcsetpgrp");
          }
        }
      }
      if (i == 0 && redir == 0) {
        FILE *f = fopen(fname, "r");
        int fd = fileno(f);
        printf("remapped input fd: %d\n", fd);
        dup2(fd, 0);
      } else if (i == num_cmds - 1 && redir == 1) {
        FILE *f = fopen(fname, "w");
        int fd = fileno(f);
        printf("remapped output fd: %d\n", fd);
        dup2(fd, 1);
      } else if (i == num_cmds - 1 && redir == 2) {
        FILE *f = fopen(fname, "a");
        int fd = fileno(f);
        printf("remapped output fd: %d\n", fd);
        dup2(fd, 1);
      }
      if (i > 0) {
        dup2(p[i-1][0], 0);
        close(p[i-1][0]);
        close(p[i-1][1]);
      }
      if (i < num_cmds - 1) {
        dup2(p[i][1], 1);
        close(p[i][0]);
        close(p[i][1]);
      }
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
      exit(2);
    }

    if (i > 0) {
      close(p[i-1][0]);
    }
    if (i < num_cmds - 1) {
      close(p[i][1]);
    }

    if (setpgid(pid, pid) == -1 && errno != EACCES) {
      perror("setpgid");
    }

    if (i == 0 && fg) {
      if (tcsetpgrp(STDIN_FILENO, getpgid(pid)) < 0) {
        perror("tcsetpgrp");
        exit(1);
      }
    }

    if (i == num_cmds - 1) {
      lastPid = pid;
    }
  }

  int status = 0;
  if (fg) {
    pid_t childpid = waitpid(lastPid, &status, 0);
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) {
      perror("tcsetpgrp");
      exit(1);
    }
    if (childpid == -1) {
      perror("waitpid");
    } else {
      int retVal = WEXITSTATUS(status);
      printf("pid: %d, status: %d\n$ ", childpid, retVal);
    }
  }

  fflush(stdout);
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
  fflush(stdout);
}

void sigIntHandler(int sig) {
  printf("\nsc number: ");
  int ind;
  scanf("%d", &ind);
  printf("%s\n", sc[ind]);
  exec_cmd(sc[ind]);
}

void sigChldHandler(int sig) {
  int status = 0;
  pid_t childpid = waitpid(-1, &status, WNOHANG);
  if (childpid < 0) {
    if (errno == ECHILD) return;
  } else {
    int retVal = WEXITSTATUS(status);
    printf("pid: %d, status: %d\n$ ", childpid, retVal);
    fflush(stdout);
  }
}

int main(int argc, char* argv[]) {
  if (signal(SIGINT, sigIntHandler) == SIG_ERR) {
    perror("signal");
  }
  signal(SIGCHLD, sigChldHandler);
  signal (SIGTTOU, SIG_IGN);
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
