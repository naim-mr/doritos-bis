/* hello.c

 *	Simple hello world program

 *

//  Copyright (c) 1999-2000 INSA de Rennes.

//  All rights reserved.  

//  See copyright_insa.h for copyright notice and limitation 

//  of liability and disclaimer of warranty provisions.

 */



#include "userlib/syscall.h"

#include "userlib/libnachos.h"
#include "userlib/syscall.h"

int v;
SemId sem;

void job(){
    P(sem);
    int i = 0;
    for(i;i<2000000;i++){
        v++;
    }
    V(sem);
}


int

main()

{
  v = 0;
  n_printf("** ** ** Bonjour le monde ** ** **\n");
  sem = SemCreate("semInc",1);
  int argumentprout = 3;
  ThreadId t1 = threadCreate("prout",job);
  ThreadId t2 = threadCreate("proutito",job);
  Join(t1);
  n_printf("1: %d\n",v);
  Join(t2);
  n_printf("2: %d\n",v);
  SemDestroy(sem);
  n_printf("3: %d\n",v);


  return 0;

}

