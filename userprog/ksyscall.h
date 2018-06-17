/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"


#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/file.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sched.h>

#include <signal.h>
#include <sys/types.h>
#include <pthread.h>

#define SHELL "/bin/sh"

static bool ReadMem(int addr, int size, int *value) {
  return kernel->machine->ReadMem(addr, size, value);
}
static bool WriteMem(int addr, int size, int value) {
  return kernel->machine->WriteMem(addr, size, value);
}

void SysHalt()
{
  kernel->interrupt->Halt();
}


int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

int SysStrncmp(int str1, int str2, int n)
{
  /*cerr << (char *)&kernel->machine->mainMemory[(int)str1] << endl <<
                 (char *)&kernel->machine->mainMemory[(int)str2] << endl;*/
  int c1, c2;
  int i = 0;
  c1 = c2 = 1;
  while (c1 == c2 && c1 && i < n)
  {
    ASSERT(ReadMem(str1 + i, 1, &c1) && ReadMem(str2 + i, 1, &c2));
    ++i;
  }
  return c1 - c2;
}

int SysWrite(int buffer, int size, OpenFileId id) {
  char buf[32];
  int c;
  int i, k;
  int p = 0;
  int count = 0;
  while (size)
  {
    for (i = 0, k = min(32, size); i < k; ++i, ++p)
    {
      ASSERT(ReadMem(buffer + p, 1, &c));
      buf[i] = (char)c;
    }
    count += write(id, buf, (size_t)k);
    size -= k;
  }

  return count;
}

int SysRead(int buffer, int size, OpenFileId id) {
  char buf[32];
  int c;
  int i, j, k;
  int p = 0;
  int count = 0;
  j = k = 0;
  while (size && k == j)
  {
    j = min(32,size);
    k = read(id, buf, (size_t)j);
    for (i = 0; i < k; ++i, ++p)
    {
      ASSERT(WriteMem(buffer + p, 1, (int)buf[i]));
    }
    count += k;
    size -= j;
  }

  return count;
}

SpaceId SysExec(int exec_name)
{
  //cerr << (char *)&kernel->machine->mainMemory[(int)exec_name] << endl;
  char buf[60];
  int c, i = 0;
  do
  {
    ASSERT(ReadMem(exec_name+i, 1, &c))
    buf[i++] = (char)c;
  } while (c && i < 60);
  if (i == 60)
    buf[59] = '\0';
    
  pid_t child;
  child = vfork();
  if (child == 0)
  {
    execl(SHELL, SHELL, "-c", buf, NULL);
    _exit(EXIT_FAILURE);
  }
  else if (child < 0)
    return EPERM;
  return (SpaceId)child;
}

int SysJoin(SpaceId id) {
  return waitpid((pid_t) id, (int*) 0, 0);
}







#endif /* ! __USERPROG_KSYSCALL_H__ */
