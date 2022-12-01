#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "file.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_socket(void)
{
  // Get the socket domain and type from the arguments.
  int domain, type, protocol;
  if (argint(0, &domain) < 0 || argint(1, &type) < 0 || argint(2, &protocol) < 0) {
    // Handle error case.
    return -1;
  }

  // Use the lwIP raw API to create a new socket.
  // You may need to grab a lock before calling any lwIP
  // functions to ensure thread safety.
  int sockfd = lwip_socket(domain, type, protocol);
  if (sockfd < 0) {
    // Handle error case.
    return -1;
  }

  // Create a new file structure for the socket.
  struct file *f;
  if ((f = filealloc()) == 0) {
    // Handle error case.
    return -1;
  }

  // Set the file type to FD_SOCK, and store a pointer
  // to the lwIP socket structure in the file's data member.
  f->type = FD_SOCK;
  f->data = (void*)sockfd;

  // Return the file descriptor for the new socket.
  return fdalloc(f);
}

int
sys_connect(void)
{
  // Get the file descriptor and sockaddr from the arguments.
  int fd, sockaddr;
  if (argint(0, &fd) < 0 || argptr(1, &sockaddr, sizeof(sockaddr)) < 0) {
    // Handle error case.
    return -1;
  }

  // Get the file structure for the socket.
  struct file *f;
  if (fd >= NOFILE || (f = myproc()->ofile[fd]) == 0 || f->type != FD_SOCK) {
    // Handle error case.
    return -1;
  }

  // Get the socket associated with the file.
  struct lwip_sock *sock = (struct lwip_sock*)f->data;

  // Use the lwIP raw API to initiate a connection to the remote host.
  // You may need to grab a lock before calling any lwIP functions
  // to ensure thread safety.
  int ret = lwip_connect(sock, sockaddr, sizeof(sockaddr));
  if (ret < 0) {
    // Handle error case.
    return -1;
  }

  // Return the status code from the connect operation.
  return ret;
}

