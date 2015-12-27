#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/* pipe */
#include <unistd.h>

/* POSIX IPC */
#include <mqueue.h>
#include <semaphore.h>
#include <sys/mman.h>

#include "dlib_comm.h"

#define FIFO1 "/tmp/fifo.1"
#define FIFO2 "/tmp/fifo.2"
#define FILE_MODE S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH

static int Echo(int argc, char** argv)
{
  printf("hello dwylkz!\n");
  return 0;
}

void Server(int fd_read, int fd_write)
{
  int ret = 0;

  char foo[BUFSIZ];

  ssize_t n = 0;
  n = read(fd_read, foo, BUFSIZ);
  if (n <= 0) {
    perror("read failed");
    return ;
  }

  foo[n] = 0;
  int fd = open(foo, O_RDONLY);
  if (fd == -1) {
    snprintf(foo, BUFSIZ, "%d: open failed: errmsg=(%s)\n", errno,
             strerror(errno));
    if (write(fd_write, foo, strlen(foo)) == -1) {
      perror("write failed");
      return ;
    }
    return ;
  }

  while ((n = read(fd, foo, BUFSIZ)) > 0) {
    if (write(fd_write, foo, n) == -1) {
      perror("write failed");
      return ;
    }
  }
  close(fd);
}

void Client(int fd_read, int fd_write)
{
  int ret = 0;

  char foo[BUFSIZ];
  if (fgets(foo, BUFSIZ, stdin) == NULL) {
    return ;
  }

  int foo_len = strlen(foo);
  if (foo[foo_len-1] == '\n') foo_len--;

  if (write(fd_write, foo, foo_len) == -1) {
    perror("write failed");
    return ;
  }

  ssize_t n = 0;
  while ((n = read(fd_read, foo, BUFSIZ)) > 0) {
    if (write(fileno(stdout), foo, n) == -1) {
      perror("write failed");
      return ;
    }
  }
}

static int PipeTest(int argc, char** argv)
{
  int ret = 0;

  int fd[2] = {0};

  ret = pipe(fd);
  if (ret != 0) {
    perror("pipe failed");
    return -1;
  }

  struct stat fd_stat[2] = {0};
  for (int i = 0; i < 2; i++) {
    ret = fstat(fd[i], &fd_stat[i]);
    if (ret != 0) {
      perror("fstat failed");
      return -1;
    }
  }

  printf("%d %d\n",
         S_ISFIFO(fd_stat[0].st_mode),
         S_ISFIFO(fd_stat[1].st_mode));

  int fd_srv[2] = {0};
  int fd_cli[2] = {0};

  ret = pipe(fd_srv);
  if (ret != 0) {
    perror("pipe failed");
    return -1;
  }

  ret = pipe(fd_cli);
  if (ret != 0) {
    perror("pipe failed");
    return -1;
  }

  pid_t child_pid = fork();
  if (child_pid == -1) {
    perror("fork failed");
    return -1;
  } else if (child_pid == 0) {
    close(fd_srv[0]);
    close(fd_cli[1]);
    Server(fd_cli[0], fd_srv[1]);
    exit(0);
  }

  close(fd_cli[0]);
  close(fd_srv[1]);
  Client(fd_srv[0], fd_cli[1]);
  waitpid(child_pid, NULL, 0);
  return 0;
}

static int FullDuplexPipeTest(int argc, char** argv)
{
  int ret = 0;

  int fd[2];

  if (pipe(fd) == -1) {
    perror("pipe failed");
    return -1;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    return -1;
  } else if (pid == 0) {
    sleep(3);

    char c;
    if (read(fd[0], &c, 1) != 1) {
      perror("child: read failed");
      return -1;
    }
    printf("child read %c\n", c);
    if (write(fd[0], &c, 1) == -1) {
      perror("child write failed");
      return -1;
    }
    return 0;
  }

  if (write(fd[1], "p", 1) == -1) {
    perror("parent write failed");
    return -1;
  }

  char c;
  if (read(fd[1], &c, 1) != 1) {
    perror("parent read failed");
    return -1;
  }
  printf("parent read %c\n", c);
  return 0;
}

static int PopenTest(int argc, char** argv)
{
  int ret = 0;

  char foo[BUFSIZ];
  if (fgets(foo, BUFSIZ, stdin) == NULL) return -1;
  int foo_len = strlen(foo);
  if (foo[foo_len-1] == '\n') foo_len--;

  char bar[BUFSIZ];
  snprintf(bar, BUFSIZ, "cat %s", foo);

  FILE* fp = popen(bar, "r");
  if (fp == NULL) {
    perror("popen failed");
    return -1;
  }

  ssize_t n = 0;
  while (fgets(foo, BUFSIZ, fp) != NULL) {
    snprintf(bar, BUFSIZ, "read: %s", foo);
    if (fputs(bar, stdout) == EOF) {
      perror("fputs failed");
      return -1;
    }
  }

  if (pclose(fp) == -1) {
    perror("pclose failed");
    return -1;
  }

  return 0;
}

static int FIFOTest(int argc, char** argv)
{
  int ret = 0;

  ret = mkfifo(FIFO1, FILE_MODE);
  if (ret == -1 && errno != EEXIST) {
    perror("mkfifo failed");
    goto err_0;
  }

  ret = mkfifo(FIFO2, FILE_MODE);
  if (ret == -1 && errno != EEXIST) {
    perror("mkfifo failed");
    goto err_1;
  }

  pid_t child_pid = fork();
  if (child_pid == -1) {
    perror("fork failed");
    return -1;
  } else if (child_pid == 0) {
    int readfd = open(FIFO1, O_RDONLY, 0);
    if (readfd == -1) {
      perror("open readfd failed");
      goto err_2;
    }

    int writefd= open(FIFO2, O_WRONLY, 0);
    if (writefd == -1) {
      perror("open writefd failed");
      goto err_2;
    }

    Server(readfd, writefd);
    return 0;
  }

  int writefd= open(FIFO1, O_WRONLY, 0);
  if (writefd == -1) {
    perror("open writefd failed");
    goto err_2;
  }

  int readfd = open(FIFO2, O_RDONLY, 0);
  if (readfd == -1) {
    perror("open readfd failed");
    goto err_2;
  }

  Client(readfd, writefd);
  waitpid(child_pid, NULL, 0);

  close(readfd);
  close(writefd);

  unlink(FIFO2);
  unlink(FIFO1);
  return 0;

err_2:
  unlink(FIFO2);
err_1:
  unlink(FIFO1);
err_0:
  return -1;
}

static int FIFOClientTest(int argc, char** argv)
{
  int ret = 0;

  int writefd= open(FIFO1, O_WRONLY, 0);
  if (writefd == -1) {
    perror("open writefd failed");
    goto err_2;
  }

  int readfd = open(FIFO2, O_RDONLY, 0);
  if (readfd == -1) {
    perror("open readfd failed");
    goto err_2;
  }

  Client(readfd, writefd);

  close(readfd);
  close(writefd);

  unlink(FIFO2);
  unlink(FIFO1);

  return 0;
err_2:
  unlink(FIFO2);
err_1:
  unlink(FIFO1);
err_0:
  return -1;
}

static int FIFOSrvTest(int argc, char** argv)
{
  int ret = 0;

  ret = mkfifo(FIFO1, FILE_MODE);
  if (ret == -1 && errno != EEXIST) {
    perror("mkfifo failed");
    goto err_0;
  }

  ret = mkfifo(FIFO2, FILE_MODE);
  if (ret == -1 && errno != EEXIST) {
    perror("mkfifo failed");
    goto err_1;
  }

  int readfd = open(FIFO1, O_RDONLY, 0);
  if (readfd == -1) {
    perror("open readfd failed");
    goto err_2;
  }

  int writefd= open(FIFO2, O_WRONLY, 0);
  if (writefd == -1) {
    perror("open writefd failed");
    goto err_2;
  }

  Server(readfd, writefd);

  return 0;
err_2:
  unlink(FIFO2);
err_1:
  unlink(FIFO1);
err_0:
  return -1;
}

int main(int argc, char** argv)
{
  const dlib_cmd_t cmds[] = {
    DLIB_CMD_DEFINE(Echo, ""),
    DLIB_CMD_DEFINE(PipeTest, ""),
    DLIB_CMD_DEFINE(FullDuplexPipeTest, ""),
    DLIB_CMD_DEFINE(PopenTest, ""),
    DLIB_CMD_DEFINE(FIFOTest, ""),
    DLIB_CMD_DEFINE(FIFOClientTest, ""),
    DLIB_CMD_DEFINE(FIFOSrvTest, ""),
    DLIB_CMD_NULL
  };
  return dlib_subcmd(argc, argv, cmds);
}
