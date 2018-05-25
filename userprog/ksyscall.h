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

void SysHalt()
{
  kernel->interrupt->Halt();
}


int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

int SysStrncmp(char *str1, char *str2, int n)
{
  /*cerr << (char *)&kernel->machine->mainMemory[(int)str1] << endl <<
                 (char *)&kernel->machine->mainMemory[(int)str2] << endl;*/
  return strncmp((char *)&kernel->machine->mainMemory[(int)str1],
                 (char *)&kernel->machine->mainMemory[(int)str2], n);
}

int SysWrite(char *buffer, int size, OpenFileId id) {
  return write(id, (char *)&kernel->machine->mainMemory[(int)buffer], (size_t) size);
}

int SysRead(char *buffer, int size, OpenFileId id) {
/*char s[9];
  int i = 0;
  do {read(id,&s[i],1);}while(s[i++]!='\n');s[--i]='\0';
  cerr << s << endl;
  SysHalt();*/  
  int r = read(id, (char *)&kernel->machine->mainMemory[(int)buffer], (size_t) size);
//cerr << (int)buffer << ':' << kernel->machine->mainMemory[(int)buffer] << ' ';
  return r;
}

SpaceId SysExec(char* exec_name) {
//cerr << (char *)&kernel->machine->mainMemory[(int)exec_name] << endl;
  pid_t child;
  child = vfork();
  if(child == 0) {
    execl (SHELL, SHELL, "-c", (char *)&kernel->machine->mainMemory[(int)exec_name], NULL);
    _exit (EXIT_FAILURE);
  } else if(child < 0)
    return EPERM;
  return (SpaceId) child;
}

int SysJoin(SpaceId id) {
  return waitpid((pid_t) id, (int*) 0, 0);
}







#endif /* ! __USERPROG_KSYSCALL_H__ */
