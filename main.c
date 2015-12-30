#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

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

#define FIFOPRE "/tmp/fifo"
#define FIFOSRV FIFOPRE".srv"
#define FIFO1 FIFOPRE".1"
#define FIFO2 FIFOPRE".2"
#define FILE_MODE S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH

#define MESSAGE_DATA_SIZE (PIPE_BUF-sizeof(uint32_t)*2)
#define MESSAGE_HEAD_SIZE (sizeof(Message)-MESSAGE_DATA_SIZE)
#define MESSAGE_ERR_OK 0
#define MESSAGE_ERR_SEND 1
#define MESSAGE_ERR_READ_HEAD 2
#define MESSAGE_ERR_READ_DATA 3
typedef struct Message
{
  uint32_t len;
  uint32_t type;
  char data[MESSAGE_DATA_SIZE];
} Message;
int MessageSend(Message* message, int fd)
{
  ssize_t total = MESSAGE_HEAD_SIZE+message->len;
  ssize_t sendbytes = dlib_so_write(fd, message, total);
  if (sendbytes != total) {
    DLIB_ERR("dlib_so_write failed: total=%ld send=%ld", total, sendbytes);
    return MESSAGE_ERR_SEND;
  }
  return MESSAGE_ERR_OK;
}
int MessageRecv(Message* message, int fd)
{
  ssize_t recvbytes = dlib_so_read(fd, message, MESSAGE_HEAD_SIZE);
  if (recvbytes == 0) return MESSAGE_ERR_OK;
  if (recvbytes != MESSAGE_HEAD_SIZE) {
    DLIB_ERR("dlib_so_read failed: headsize=%ld recv=%ld", MESSAGE_HEAD_SIZE, recvbytes);
    return MESSAGE_ERR_READ_HEAD;
  }
  if (message->len == 0) return 0;
  recvbytes = dlib_so_read(fd, message+MESSAGE_HEAD_SIZE, message->len);
  if (recvbytes != message->len) {
    DLIB_ERR("dlib_so_read failed: datalen=%u recv=%ld", message->len, recvbytes);
    return MESSAGE_ERR_READ_DATA;
  }
  return MESSAGE_ERR_OK;
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

static int Echo(int argc, char** argv)
{
  DLIB_INFO("hello dwylkz!");
  DLIB_INFO("%d", BUFSIZ);
  DLIB_INFO("%d", EOF);
  DLIB_INFO("%d", PIPE_BUF);
  return 0;
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

  /* non block test
  int flag = fcntl(fd_srv[0], F_GETFL);
  if (flag == -1) {
    perror("F_GETFL failed");
    return -1;
  }
  printf("flag=%d\n", flag);
  if (fcntl(fd_srv[0], F_SETFL, flag|O_NONBLOCK) == -1) {
    perror("F_SETFL failed");
    return -1;
  }
  */

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

  struct stat readstat;
  fstat(readfd, &readstat);
  printf("%d\n", S_ISFIFO(readstat.st_mode));

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

static int FIFOMultiSrvTest(int argc, char** argv)
{
  int ret = 0;

  ret = mkfifo(FIFOSRV, FILE_MODE);
  if (ret == -1 && errno != EEXIST) {
    perror("mkfifo failed");
    goto err_0;
  }

  int readfd = open(FIFOSRV, O_RDONLY, 0);
  if (readfd == -1) {
    perror("open read fifo failed");
    goto err_1;
  }

  int dummy = open(FIFOSRV, O_WRONLY, 0);
  if (dummy == -1) {
    perror("open write fifo failed");
    goto err_2;
  }

  char fromcli[BUFSIZ];
  ssize_t n = 0;
  while ((n = dlib_so_read(readfd, fromcli, BUFSIZ)) > 0) {
    if (fromcli[n-1] == '\n') fromcli[n-1] = '\0';
    DLIB_INFO("fromcli=(%s)", fromcli);
    char* space_pos = strchr(fromcli, ' ');
    if (space_pos == NULL) {
      perror("invalid format");
      continue;
    }
    *space_pos = '\0';
    const char* fifo_name = fromcli;
    DLIB_INFO("before open: fifo_name=(%s)", fifo_name);
    int writefd = open(fifo_name, O_WRONLY, 0);
    if (writefd == -1) {
      perror("open write failed");
      continue;
    }
    DLIB_INFO("after open");

    const char* file_name = space_pos+1;
    DLIB_INFO("file_name=(%s)", file_name);
    int filefd = open(file_name, O_RDONLY);
    if (filefd == -1) {
      char errbuf[BUFSIZ];
      snprintf(errbuf, BUFSIZ, "%d: open file failed: filename=(%s) msg=(%s)\n", errno,
               file_name,
               strerror(errno));
      DLIB_ERR("%s", errbuf);
      if (write(writefd, errbuf, strlen(errbuf)) == -1) {
        perror("write failed");
      }
      close(writefd);
      continue;
    }

    char filebuf[BUFSIZ] = {0};
    while ((n = read(filefd, filebuf, BUFSIZ)) > 0) {
      if (dlib_so_write(writefd, filebuf, n) == -1) {
        perror("write failed");
        continue;
      }
    }

    close(filefd);
    close(writefd);
  }

  close(dummy);
  close(readfd);
  unlink(FIFOSRV);
  return 0;
err_2:
  close(readfd);
err_1:
  unlink(FIFOSRV);
err_0:
  return -1;
}

static int FIFOMultiCliTest(int argc, char** argv)
{
  int ret = 0;

  char fifoname[BUFSIZ];
  snprintf(fifoname, BUFSIZ, "%s.%d", FIFOPRE, getpid());
  ret = mkfifo(fifoname, FILE_MODE);
  if (ret == -1 && errno != EEXIST) {
    perror("mkfifo failed");
    goto err_0;
  }

  int writefd = open(FIFOSRV, O_WRONLY, 0);
  if (writefd == -1) {
    perror("open writefd failed");
    goto err_1;
  }

  char filepath[BUFSIZ] = {0};
  if (argc > 1) {
    snprintf(filepath, BUFSIZ, "%s\n", argv[1]);
  } else if (fgets(filepath, BUFSIZ, stdin) == NULL) {
    goto err_1;
  }

  char bar[BUFSIZ];
  snprintf(bar, BUFSIZ, "%s %s", fifoname, filepath);

  if (write(writefd, bar, strlen(bar)) == -1) {
    perror("write failed");
    goto err_1;
  }

  int readfd = open(fifoname, O_RDONLY, 0);
  if (readfd == -1) {
    perror("open read failed");
    goto err_1;
  }

  char filebuf[BUFSIZ] = {0};
  ssize_t n = 0;
  while ((n = dlib_so_read(readfd, filebuf, BUFSIZ)) > 0) {
    if (dlib_so_write(fileno(stdout), filebuf, n) == -1) {
      DLIB_ERR("write to stdout failed");
    }
  }

  close(writefd);
  close(readfd);
  unlink(fifoname);
  return 0;
err_1:
  unlink(fifoname);
err_0:
  return -1;
}

static int MultiFIFOMultiCliTest(int argc, char** argv)
{
  return dlib_subcmd_mutiplex(argc, argv, FIFOMultiCliTest);
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
    DLIB_CMD_DEFINE(FIFOMultiSrvTest, ""),
    DLIB_CMD_DEFINE(FIFOMultiCliTest, ""),
    DLIB_CMD_DEFINE(MultiFIFOMultiCliTest, ""),
    DLIB_CMD_NULL
  };
  return dlib_subcmd(argc, argv, cmds);
}
