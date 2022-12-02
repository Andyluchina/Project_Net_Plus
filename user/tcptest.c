//
// test program for the alarm lab.
// you can modify this file for testing,
// but please make sure your kernel
// modifications pass the original
// versions of these tests.
//

#include "lwip/include/lwip/tcp.h"

void test0();

int
main(int argc, char *argv[])
{
  test0();
  return 0;
}
// tests whether the kernel calls
// the alarm handler even a single time.
void
test0()
{
  printf("test0 start\n");
  // new tcp connection
  struct tcp_pcb *pcb = tcp_new();
  // set the alarm handler
  pcb = pcb;
}
