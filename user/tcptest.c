#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"
// #include "lwip/tcp.h"

int
main(int argc, char *argv[])
{
  printf("TCP Test\n");
  tcpcall(1);
  exit(1);
}
