#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

#include "dlib_comm.h"

static int echo(int argc, char** argv)
{
  printf("hello dwylkz!\n");
  return 0;
}

int main(int argc, char** argv)
{
  const dlib_cmd_t cmds[] = {
    DLIB_CMD_DEFINE(echo, ""),
    DLIB_CMD_NULL
  };
  return dlib_subcmd(argc, argv, cmds);
}
