/*! \file exception.cc 

 //  \brief Entry point into the Nachos kernel .

 //

 //    There are two kinds of things that can cause control to

 //    transfer back to here:

 //

 //    syscall -- The user code explicitly requests to call a Nachos

 //    system call

 //

 //    exceptions -- The user code does something that the CPU can't handle.

 //    For instance, accessing memory that doesn't exist, arithmetic errors,

 //    etc.  

 //

 //    Interrupts (which can also cause control to transfer from user

 //    code into the Nachos kernel) are handled elsewhere.

 */

// Copyright (c) 1992-1993 The Regents of the University of California.

// All rights reserved.  See copyright.h for copyright notice and limitation

// of liability and disclaimer of warranty provisions.

#include "drivers/drvACIA.h"
#include "drivers/drvConsole.h"
#include "filesys/oftable.h"
#include "kernel/msgerror.h"
#include "kernel/synch.h"
#include "kernel/system.h"
#include "machine/machine.h"
#include "userlib/syscall.h"
#include "utility/objid.h"
#include "vm/pagefaultmanager.h"

//----------------------------------------------------------------------

// GetLengthParam

/*! Returns the length of a string stored in the machine memory,

//    including the '\0' terminal

//

// \param addr is the memory address of the string */

//----------------------------------------------------------------------

static int GetLengthParam(int addr) {
    int i = 0;

    uint32_t c = -1;

    // Scan the string until the null character is found

    while (c != 0) {
        g_machine->mmu->ReadMem(addr++, 1, &c, false);

        i++;
    }

    return i + 1;
}

//----------------------------------------------------------------------

// GetStringParam

/*!	Copies a string from the machine memory

//

//	\param addr is the memory address of the string

//	\param dest is where the string is going to be copied

//      \param maxlen maximum length of the string to copy in dest,

//        including the trailing '\0' 

*/

//----------------------------------------------------------------------

static void GetStringParam(int addr, char *dest, int maxlen) {
    int i = 0;

    uint32_t c = -1;

    while ((c != 0) && (i < maxlen)) {
        // Read a character from the machine memory

        g_machine->mmu->ReadMem(addr++, 1, &c, false);

        // Put it in the kernel memory

        dest[i++] = (char)c;
    }

    // Force a \0 at the end

    dest[maxlen - 1] = '\0';
}

//----------------------------------------------------------------------

// ExceptionHandler

/*!   Entry point into the Nachos kernel.  Called when a user program

 //    is executing, and either does a syscall, or generates an addressing

 //    or arithmetic exception.

 //

 //    For system calls, the calling convention is the following:

 //

 //    - system call identifier -- r2

 //    - arg1 -- r4

 //    - arg2 -- r5

 //    - arg3 -- r6

 //    - arg4 -- r7

 //

 //    The result of the system call, if any, must be put back into r2. 

 //

 //    \param exceptiontype is the kind of exception.

 //           The list of possible exception are defined in machine.h.

 //    \param vaddr is the address that causes the exception to occur

 //           (when used)  

 */

//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType exceptiontype, int vaddr)

{
    // Get the content of the r2 register (system call number in case

    // of a system call

    int type = g_machine->ReadIntRegister(2);

    switch (exceptiontype) {
        case NO_EXCEPTION:

            printf("Nachos internal error, a NoException exception is raised ...\n");

            g_machine->interrupt->Halt(0);

            break;

        case SYSCALL_EXCEPTION: {
            // System calls

            // -------------

            switch (type) {
                char msg[MAXSTRLEN];  // Argument for the PError system call

                    // You will find below all Nachos system calls ...

                case SC_HALT:

                    // The halt system call. Stops Nachos.

                    DEBUG('e', (char *)"Shutdown, initiated by user program.\n");

                    g_machine->interrupt->Halt(0);

                    g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    return;

                case SC_SYS_TIME: {
                    // The systime system call. Gets the system time

                    DEBUG('e', (char *)"Systime call, initiated by user program.\n");

                    int addr = g_machine->ReadIntRegister(4);

                    uint64_t tick = g_stats->getTotalTicks();

                    uint32_t seconds = (uint32_t)

                        cycle_to_sec(tick, g_cfg->ProcessorFrequency);

                    uint32_t nanos = (uint32_t)

                        cycle_to_nano(tick, g_cfg->ProcessorFrequency);

                    g_machine->mmu->WriteMem(addr, sizeof(uint32_t), seconds);

                    g_machine->mmu->WriteMem(addr + 4, sizeof(uint32_t), nanos);

                    g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    break;
                }

                case SC_EXIT: {
                    // The exit system call

                    // Ends the calling thread

                    DEBUG('e', (char *)"Thread 0x%x %s exit call.\n", g_current_thread, g_current_thread->GetName());

                    ASSERT(g_current_thread->type == THREAD_TYPE);

                    g_current_thread->Finish();

                    break;
                }

                case SC_EXEC: {
                    // The exec system call

                    // Creates a new process (thread+address space)

                    DEBUG('e', (char *)"Process: Exec call.\n");

                    int addr;

                    int size;

                    char name[MAXSTRLEN];

                    int error = NO_ERROR;

                    // Get the process name
                    
                    addr = g_machine->ReadIntRegister(4);

                    size = GetLengthParam(addr);

                    char ch[size];

                    GetStringParam(addr, ch, size);
                    
                    sprintf(name, "master thread of process %s", ch);
                
                    Process *p = new Process(ch, &error);
                    
                    if (error != NO_ERROR) {
                        g_machine->WriteIntRegister(2, ERROR);

                        if (error == OUT_OF_MEMORY)

                            g_syscall_error->SetMsg((char *)"", error);

                        else

                            g_syscall_error->SetMsg(ch, error);

                        break;
                    }
                    
                    Thread *ptThread = new Thread(name);

                    int32_t tid = g_object_ids->AddObject(ptThread);

                    error = ptThread->Start(p,

                                            p->addrspace->getCodeStartAddress(),

                                            -1);

                    if (error != NO_ERROR) {
                        g_machine->WriteIntRegister(2, ERROR);

                        if (error == OUT_OF_MEMORY)

                            g_syscall_error->SetMsg((char *)"", error);

                        else

                            g_syscall_error->SetMsg(name, error);

                        break;
                    }
                    
                    g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    g_machine->WriteIntRegister(2, tid);
                    
                    break;
                }

                case SC_NEW_THREAD: {
                    // The newThread system call

                    // Create a new thread in the same address space

                    DEBUG('e', (char *)"Multithread: NewThread call.\n");

                    Thread *ptThread;

                    int name_addr;

                    int32_t fun;

                    int arg;

                    int err = NO_ERROR;

                    // Get the address of the string for the name of the thread

                    name_addr = g_machine->ReadIntRegister(4);

                    // Get the pointer to the function to be executed by the new thread

                    fun = g_machine->ReadIntRegister(5);

                    // Get the function parameters

                    arg = g_machine->ReadIntRegister(6);

                    // Build the name of the thread

                    int size = GetLengthParam(name_addr);

                    char thr_name[size];

                    GetStringParam(name_addr, thr_name, size);

                    //char *proc_name = g_current_thread->getProcessOwner()->getName();

                    // Finally start it

                    ptThread = new Thread(thr_name);

                    int32_t tid;

                    tid = g_object_ids->AddObject(ptThread);

                    err = ptThread->Start(g_current_thread->GetProcessOwner(),

                                          fun, arg);

                    if (err != NO_ERROR) {
                        g_machine->WriteIntRegister(2, ERROR);

                        g_syscall_error->SetMsg((char *)"", err);

                    }

                    else {
                        g_machine->WriteIntRegister(2, tid);

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    }

                    break;
                }

                case SC_JOIN: {
                    // The join system call

                    // Wait for the thread idThread to finish

                    DEBUG('e', (char *)"Process or thread: Join call.\n");

                    int32_t tid;

                    Thread *ptThread;

                    tid = g_machine->ReadIntRegister(4);

                    ptThread = (Thread *)g_object_ids->SearchObject(tid);

                    if (ptThread

                        && ptThread->type == THREAD_TYPE)

                    {
                        g_current_thread->Join(ptThread);

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);

                    }

                    else

                    // Thread already terminated (type set to INVALID_TYPE) or call on an object

                    // that is not a thread

                    // Exit with no error code since we cannot separate the two cases

                    {
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    }

                    DEBUG('e', (char *)"Fin Join");

                    break;
                }

                case SC_YIELD: {
                    DEBUG('e', (char *)"Process or thread: Yield call.\n");

                    ASSERT(g_current_thread->type == THREAD_TYPE);

                    g_current_thread->Yield();

                    g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    break;
                }

                case SC_PERROR: {
                    // the PError system call

                    // print the last error message

                    DEBUG('e', (char *)"Debug: Perror call.\n");

                    int size;

                    int addr;

                    addr = g_machine->ReadIntRegister(4);

                    size = GetLengthParam(addr);

                    char ch[size];

                    GetStringParam(addr, ch, size);

                    g_syscall_error->PrintLastMsg(g_console_driver, ch);

                    break;
                }

#ifdef ETUDIANTS_TP
                case SC_P: {
                    DEBUG('e', (char *)"Semaphore : P.\n");

                    Semaphore *pSem;
                    int sem_id = g_machine->ReadIntRegister(4);
                    pSem = (Semaphore *)g_object_ids->SearchObject(sem_id);

                    if (pSem && pSem->type == SEMAPHORE_TYPE) {
                        pSem->P();

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_SEMAPHORE_ID);

                        g_machine->WriteIntRegister(2,  INVALID_SEMAPHORE_ID);
                    }
                    break;
                }

                case SC_V: {
                    DEBUG('e', (char *)"Semaphore : V.\n");

                    Semaphore *vSem;
                    int sem_id = g_machine->ReadIntRegister(4);
                    vSem = (Semaphore *)g_object_ids->SearchObject(sem_id);

                    if (vSem && vSem->type == SEMAPHORE_TYPE) {
                        vSem->V();
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_SEMAPHORE_ID);

                        g_machine->WriteIntRegister(2, INVALID_SEMAPHORE_ID);
                    }
                    break;
                }

                case SC_SEM_CREATE: {
                    DEBUG('e', (char *)"Semaphore : Create.\n");

                    Semaphore *sem;
                    int name_addr = g_machine->ReadIntRegister(4);
                    int value = g_machine->ReadIntRegister(5);

                    int size = GetLengthParam(name_addr);

                    char sem_name[size];

                    GetStringParam(name_addr, sem_name, size);

                    sem = new Semaphore(sem_name, value);
                    int sem_id=g_object_ids->AddObject(sem);
                    g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    g_machine->WriteIntRegister(2,sem_id);
                    break;
                }

                case SC_SEM_DESTROY: {
                    DEBUG('e', (char *)"Semaphore : Destroy.\n");

                    Semaphore *sem;
                    int sem_id = g_machine->ReadIntRegister(4);
                    sem = (Semaphore *)g_object_ids->SearchObject(sem_id);

                    if (sem && sem->type == SEMAPHORE_TYPE) {
                        delete sem;
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"",  INVALID_SEMAPHORE_ID);

                        g_machine->WriteIntRegister(2,  INVALID_SEMAPHORE_ID);
                    }
                    break;
                }

                case SC_LOCK_CREATE: {
                    DEBUG('e', (char *)"Lock : Create.\n");

                    Lock *lock;
                    int name_addr = g_machine->ReadIntRegister(4);

                    int size = GetLengthParam(name_addr);

                    char lock_name[size];

                    GetStringParam(name_addr, lock_name, size);

                    lock = new Lock(lock_name);
                    int l_id=g_object_ids->AddObject(lock);
                    g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    g_machine->WriteIntRegister(2,l_id);
                    break;
                }

                case SC_LOCK_DESTROY: {
                    DEBUG('e', (char *)"Lock : Destroy.\n");

                    Lock *lock;
                    int lock_id = g_machine->ReadIntRegister(4);
                    lock = (Lock *)g_object_ids->SearchObject(lock_id);
                    if (lock && lock->type == LOCK_TYPE) {
                        delete lock;
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"",  INVALID_LOCK_ID);

                        g_machine->WriteIntRegister(2,  INVALID_LOCK_ID);
                    }
                    break;
                }

                case SC_LOCK_ACQUIRE: {
                    DEBUG('e', (char *)"Lock : Acquire.\n");

                    Lock *lock;
                    int lock_id = g_machine->ReadIntRegister(4);
                    lock = (Lock *)g_object_ids->SearchObject(lock_id);

                    if (lock && lock->type == LOCK_TYPE) {
                        lock->Acquire();
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_LOCK_ID);

                        g_machine->WriteIntRegister(2, INVALID_LOCK_ID);
                    }
                    break;
                }

                case SC_LOCK_RELEASE: {
                    DEBUG('e', (char *)"Lock : Release.\n");

                    Lock *lock;
                    int lock_id = g_machine->ReadIntRegister(4);
                    lock = (Lock *)g_object_ids->SearchObject(lock_id);

                    if (lock && lock->type == LOCK_TYPE) {
                        lock->Release();
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_LOCK_ID);

                        g_machine->WriteIntRegister(2, INVALID_LOCK_ID);
                    }
                    break;
                }

                case SC_COND_CREATE: {
                    DEBUG('e', (char *)"Condition : Create.\n");

                    Condition *cond;
                    int name_addr = g_machine->ReadIntRegister(4);

                    int size = GetLengthParam(name_addr);

                    char cond_name[size];

                    GetStringParam(name_addr, cond_name, size);
                    
                    
                    
                    cond = new Condition(cond_name);
                    int c_id= g_object_ids->AddObject(cond);
                    g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    g_machine->WriteIntRegister(2,c_id);
                    break;
                }

                case SC_COND_DESTROY: {
                    DEBUG('e', (char *)"Condition : Destroy.\n");

                    Condition *cond;
                    int cond_id = g_machine->ReadIntRegister(4);
                    cond = (Condition *)g_object_ids->SearchObject(cond_id);
                    if (cond && cond->type == CONDITION_TYPE) {
                        delete cond;
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_CONDITION_ID);

                        g_machine->WriteIntRegister(2, INVALID_CONDITION_ID);
                    }
                    break;
                }

                case SC_COND_WAIT: {
                    DEBUG('e', (char *)"Condition : Wait.\n");

                    Condition *cond;
                    int cond_id = g_machine->ReadIntRegister(4);
                    cond = (Condition *)g_object_ids->SearchObject(cond_id);

                    if (cond && cond->type == CONDITION_TYPE) {
                        cond->Wait();
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_CONDITION_ID);

                        g_machine->WriteIntRegister(2, INVALID_CONDITION_ID);
                    }
                    break;
                }

                case SC_COND_SIGNAL: {
                    DEBUG('e', (char *)"Condition : Signal.\n");

                    Condition *cond;
                    int cond_id = g_machine->ReadIntRegister(4);
                    cond = (Condition *)g_object_ids->SearchObject(cond_id);

                    if (cond && cond->type == CONDITION_TYPE) {
                        cond->Signal();
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_CONDITION_ID);

                        g_machine->WriteIntRegister(2, INVALID_CONDITION_ID);
                    }
                    break;
                }

                case SC_COND_BROADCAST: {
                    DEBUG('e', (char *)"Condition : Broadcast.\n");

                    Condition *cond;
                    int cond_id = g_machine->ReadIntRegister(4);
                    cond = (Condition *)g_object_ids->SearchObject(cond_id);

                    if (cond && cond->type == CONDITION_TYPE) {
                        cond->Broadcast();
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        g_machine->WriteIntRegister(2, 0);
                    } else {
                        g_syscall_error->SetMsg((char *)"", INVALID_CONDITION_ID);

                        g_machine->WriteIntRegister(2, INVALID_CONDITION_ID);
                    }
                    break;
                }
#endif

                case SC_CREATE: {
                    // The create system call

                    // Create a new file in nachos file system

                    DEBUG('e', (char *)"Filesystem: Create call.\n");

                    int addr;

                    int size;

                    int ret;

                    int sizep;

                    // Get the name and initial size of the new file

                    addr = g_machine->ReadIntRegister(4);

                    size = g_machine->ReadIntRegister(5);

                    sizep = GetLengthParam(addr);

                    char ch[sizep];

                    GetStringParam(addr, ch, sizep);

                    // Try to create it

                    int err = g_file_system->Create(ch, size);

                    if (err == NO_ERROR) {
                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        ret = 0;

                    }

                    else {
                        ret = ERROR;

                        if (err == OUT_OF_DISK)
                            g_syscall_error->SetMsg((char *)"", err);

                        else
                            g_syscall_error->SetMsg(ch, err);
                    }

                    g_machine->WriteIntRegister(2, ret);

                    break;
                }

                case SC_OPEN: {
                    // The open system call

                    // Opens a file and returns an openfile identifier

                    DEBUG('e', (char *)"Filesystem: Open call.\n");

                    int addr;

                    int ret;

                    int sizep;

                    // Get the file name

                    addr = g_machine->ReadIntRegister(4);

                    sizep = GetLengthParam(addr);

                    char ch[sizep];

                    GetStringParam(addr, ch, sizep);

                    // Try to open the file

                    OpenFile *file = g_open_file_table->Open(ch);

                    int32_t fid;

                    if (file == NULL) {
                        ret = ERROR;

                        g_syscall_error->SetMsg(ch, OPENFILE_ERROR);

                    }

                    else {
                        fid = g_object_ids->AddObject(file);

                        ret = fid;

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    }

                    g_machine->WriteIntRegister(2, ret);

                    break;
                }

                case SC_READ: {
                    // The read system call

                    // Read in a file or the console

                    DEBUG('e', (char *)"Filesystem: Read call.\n");

                    int addr;

                    int size;

                    int32_t f;

                    int numread;

                    // Get the buffer address in the machine memory

                    addr = g_machine->ReadIntRegister(4);

                    // Get the requested size

                    size = g_machine->ReadIntRegister(5);

                    // Get the openfile number or 0 (console)

                    f = g_machine->ReadIntRegister(6);

                    char buffer[size];

                    // Read in a file

                    if (f != CONSOLE_INPUT) {
                        int32_t fid = f;

                        OpenFile *file = (OpenFile *)g_object_ids->SearchObject(fid);

                        if (file && file->type == FILE_TYPE)

                        {
                            numread = file->Read(buffer, size);

                            g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        } 

                        else

                        {
                            numread = ERROR;

                            sprintf(msg, "%d", f);

                            g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
                        }

                    }

                    // Read on the console

                    else {
                        g_console_driver->GetString(buffer, size);

                        numread = size;

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    }

                    for (int i = 0; i < numread; i++)

                    {  //copy the buffer into the emulator memory

                        g_machine->mmu->WriteMem(addr++, 1, buffer[i]);
                    }

                    g_machine->WriteIntRegister(2, numread);

                    break;
                }

                case SC_WRITE: {
                    // The write system call

                    // Write in a file or at the console

                    DEBUG('e', (char *)"Filesystem: Write call.\n");

                    uint32_t addr;

                    int size;

                    int32_t f;

                    uint32_t c;

                    addr = g_machine->ReadIntRegister(4);

                    size = g_machine->ReadIntRegister(5);

                    //f is the openfileid or 1 (console)

                    f = g_machine->ReadIntRegister(6);

                    char buffer[size];

                    for (int i = 0; i < size; i++) {
                        g_machine->mmu->ReadMem(addr++, 1, &c, false);

                        buffer[i] = c;
                    }

                    int numwrite;

                    // Write in a file

                    if (f > CONSOLE_OUTPUT) {
                        int32_t fid = f;

                        OpenFile *file = (OpenFile *)g_object_ids->SearchObject(fid);

                        if (file && file->type == FILE_TYPE)

                        {
                            //write in file

                            numwrite = file->Write(buffer, size);

                            g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        }

                        else

                        {
                            numwrite = ERROR;

                            sprintf(msg, "%d", f);

                            g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
                        }

                    }

                    // write at the console

                    else {
                        if (f == CONSOLE_OUTPUT) {
                            g_console_driver->PutString(buffer, size);

                            numwrite = size;

                            g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        }

                        else {
                            numwrite = ERROR;

                            sprintf(msg, "%d", f);

                            g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
                        }
                    }

                    g_machine->WriteIntRegister(2, numwrite);

                    break;
                }

                case SC_SEEK: {
                    // Seek to a given position in an opened file

                    DEBUG('e', (char *)"Filesystem: Seek call.\n");

                    int offset;

                    int32_t f;

                    int error = NO_ERROR;

                    // Get the offset into the file

                    offset = g_machine->ReadIntRegister(4);

                    // Get the openfile number or 1 (console)

                    f = g_machine->ReadIntRegister(5);

                    // Seek into a file

                    if (f > CONSOLE_OUTPUT) {
                        int32_t fid = f;

                        OpenFile *file = (OpenFile *)g_object_ids->SearchObject(fid);

                        if (file && file->type == FILE_TYPE)

                        {
                            file->Seek(offset);

                            g_syscall_error->SetMsg((char *)"", NO_ERROR);

                        }

                        else

                        {
                            error = ERROR;

                            sprintf(msg, "%d", f);

                            g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
                        }

                        g_machine->WriteIntRegister(2, error);

                    }

                    else {
                        g_machine->WriteIntRegister(2, ERROR);

                        sprintf(msg, "%d", f);

                        g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
                    }

                    break;
                }

                case SC_CLOSE: {
                    // The close system call

                    // Close a file

                    DEBUG('e', (char *)"Filesystem: Close call.\n");

                    // Get the openfile number

                    int32_t fid = g_machine->ReadIntRegister(4);

                    OpenFile *file = (OpenFile *)g_object_ids->SearchObject(fid);

                    if (file && file->type == FILE_TYPE) {
                        g_open_file_table->Close(file->GetName());

                        g_object_ids->RemoveObject(fid);

                        delete file;

                        g_machine->WriteIntRegister(2, 0);

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    }

                    else {
                        g_machine->WriteIntRegister(2, ERROR);

                        sprintf(msg, "%d", fid);

                        g_syscall_error->SetMsg(msg, INVALID_FILE_ID);
                    }

                    break;
                }

                case SC_REMOVE: {
                    // The Remove system call

                    // Remove a file from the file system

                    DEBUG('e', (char *)"Filesystem: Remove call.\n");

                    int ret;

                    int addr;

                    int sizep;

                    // Get the name of the file to be removes

                    addr = g_machine->ReadIntRegister(4);

                    sizep = GetLengthParam(addr);

                    char *ch = new char[sizep];

                    GetStringParam(addr, ch, sizep);

                    // Actually remove it

                    int err = g_open_file_table->Remove(ch);

                    if (err == NO_ERROR) {
                        ret = 0;

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    }

                    else {
                        ret = ERROR;

                        g_syscall_error->SetMsg(ch, err);
                    }

                    g_machine->WriteIntRegister(2, ret);

                    break;
                }

                case SC_MKDIR: {
                    // the Mkdir system call

                    // make a new directory in the file system

                    DEBUG('e', (char *)"Filesystem: Mkdir call.\n");

                    int addr;

                    int sizep;

                    addr = g_machine->ReadIntRegister(4);

                    sizep = GetLengthParam(addr);

                    char name[sizep];

                    GetStringParam(addr, name, sizep);

                    // name is the name of the new directory

                    int good = g_file_system->Mkdir(name);

                    if (good != NO_ERROR) {
                        g_machine->WriteIntRegister(2, ERROR);

                        if (good == OUT_OF_DISK)
                            g_syscall_error->SetMsg((char *)"", good);

                        else
                            g_syscall_error->SetMsg(name, good);

                    }

                    else {
                        g_machine->WriteIntRegister(2, ((int)good));

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    }

                    break;
                }

                case SC_RMDIR: {
                    // the Rmdir system call

                    // remove a directory from the file system

                    DEBUG('e', (char *)"Filesystem: Rmdir call.\n");

                    int addr;

                    int sizep;

                    addr = g_machine->ReadIntRegister(4);

                    sizep = GetLengthParam(addr);

                    char name[sizep];

                    GetStringParam(addr, name, sizep);

                    int good = g_file_system->Rmdir(name);

                    if (good != NO_ERROR) {
                        g_machine->WriteIntRegister(2, ERROR);

                        g_syscall_error->SetMsg(name, good);

                    }

                    else {
                        g_machine->WriteIntRegister(2, ((int)good));

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);
                    }

                    break;
                }

                case SC_FSLIST: {
                    // The FSList system call

                    // Lists all the file and directories in the filesystem

                    g_file_system->List();

                    g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    break;
                }

                case SC_TTY_SEND: {
                    // the TtySend system call

                    // Sends some char by the serial line emulated

                    DEBUG('e', (char *)"ACIA: Send call.\n");

                    if (g_cfg->ACIA != ACIA_NONE) {
                        int result;

                        uint32_t c;

                        int i;

                        uint32_t addr = g_machine->ReadIntRegister(4);

                        char buff[MAXSTRLEN];

                        for (i = 0;; i++)

                        {
                            g_machine->mmu->ReadMem(addr + i, 1, &c, false);

                            buff[i] = (char)c;

                            if (buff[i] == '\0') break;
                        }

                        result = g_acia_driver->TtySend(buff);

                        g_machine->WriteIntRegister(2, result);

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    }

                    else {
                        g_machine->WriteIntRegister(2, ERROR);

                        g_syscall_error->SetMsg((char *)"", NO_ACIA);
                    }

                    break;
                }

                case SC_TTY_RECEIVE: {
                    // the TtyReceive system call

                    // read some char on the serial line

                    DEBUG('e', (char *)"ACIA: Receive call.\n");

                    if (g_cfg->ACIA != ACIA_NONE) {
                        int result;

                        int i = 0;

                        int addr = g_machine->ReadIntRegister(4);

                        int length = g_machine->ReadIntRegister(5);

                        char buff[length + 1];

                        result = g_acia_driver->TtyReceive(buff, length);

                        while ((i <= length)) {
                            g_machine->mmu->WriteMem(addr, 1, buff[i]);

                            addr++;

                            i++;
                        }

                        g_machine->mmu->WriteMem(addr, 1, 0);

                        g_machine->WriteIntRegister(2, result);

                        g_syscall_error->SetMsg((char *)"", NO_ERROR);

                    }

                    else {
                        g_machine->WriteIntRegister(2, ERROR);

                        g_syscall_error->SetMsg((char *)"", NO_ACIA);
                    }

                    break;
                }

                default:

                    printf("Invalid system call number : %d\n", type);

                    exit(ERROR);

                    break;
            }
        }

            // from now, the code is executed whatever system call is invoked

            // we increment the PC counter

            g_machine->int_registers[PREVPC_REG] = g_machine->int_registers[PC_REG];

            g_machine->int_registers[PC_REG] = g_machine->int_registers[NEXTPC_REG];

            g_machine->int_registers[NEXTPC_REG] += 4;

            break;

            // Other exceptions

            // ----------------

        case READONLY_EXCEPTION:

            printf("FATAL USER EXCEPTION (Thread %s, PC=0x%x):\n",

                   g_current_thread->GetName(), g_machine->ReadIntRegister(PC_REG));

            printf("\t*** Write to virtual address 0x%x on read-only page ***\n",

                   vaddr);

            g_machine->interrupt->Halt(ERROR);

            break;

        case BUSERROR_EXCEPTION:

            printf("FATAL USER EXCEPTION (Thread %s, PC=0x%x):\n",

                   g_current_thread->GetName(), g_machine->ReadIntRegister(PC_REG));

            printf("\t*** Bus error on access to virtual address 0x%x ***\n",

                   vaddr);

            g_machine->interrupt->Halt(ERROR);

            break;

        case ADDRESSERROR_EXCEPTION:

            printf("FATAL USER EXCEPTION (Thread %s, PC=0x%x):\n",

                   g_current_thread->GetName(), g_machine->ReadIntRegister(PC_REG));

            printf("\t*** Access to invalid or unmapped virtual address 0x%x ***\n",

                   vaddr);

            g_machine->interrupt->Halt(ERROR);

            break;

        case OVERFLOW_EXCEPTION:

            printf("FATAL USER EXCEPTION (Thread %s, PC=0x%x):\n",

                   g_current_thread->GetName(), g_machine->ReadIntRegister(PC_REG));

            printf("\t*** Overflow exception at address 0x%x ***\n",

                   vaddr);

            g_machine->interrupt->Halt(ERROR);

            break;

        case ILLEGALINSTR_EXCEPTION:

            printf("FATAL USER EXCEPTION (Thread %s, PC=0x%x):\n",

                   g_current_thread->GetName(), g_machine->ReadIntRegister(PC_REG));

            printf("\t*** Illegal instruction at virtual address 0x%x ***\n", vaddr);

            g_machine->interrupt->Halt(ERROR);

            break;

        case PAGEFAULT_EXCEPTION:

            ExceptionType e;

            e = g_page_fault_manager->PageFault(vaddr / g_cfg->PageSize);

            if (e != NO_EXCEPTION) {
                printf("\t*** Page fault handling failed, ... exiting\n");

                g_machine->interrupt->Halt(ERROR);
            }

            break;

        default:

            printf("Unknown exception %d\n", exceptiontype);

            g_machine->interrupt->Halt(ERROR);

            break;
    }
}
