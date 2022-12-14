/*! \file pagefaultmanager.cc

Routines for the page fault managerPage Fault Manager

*/

//

//  Copyright (c) 1999-2000 INSA de Rennes.

//  All rights reserved.

//  See copyright_insa.h for copyright notice and limitation

//  of liability and disclaimer of warranty provisions.

//

#include "vm/pagefaultmanager.h"

#include "kernel/thread.h"
#include "vm/physMem.h"
#include "vm/swapManager.h"

PageFaultManager::PageFaultManager() {
}

// PageFaultManager::~PageFaultManager()

/*! Nothing for now

*/

PageFaultManager::~PageFaultManager() {
}

// ExceptionType PageFault(uint32_t virtualPage)

/*!

//	This method is called by the Memory Management Unit when there is a

//      page fault. This method loads the page from :

//      - read-only sections (text,rodata) $\Rightarrow$ executive

//        file

//      - read/write sections (data,...) $\Rightarrow$ executive

//        file (1st time only), or swap file

//      - anonymous mappings (stack/bss) $\Rightarrow$ new

//        page from the MemoryManager (1st time only), or swap file

//

//	\param virtualPage the virtual page subject to the page fault

//	  (supposed to be between 0 and the

//        size of the address space, and supposed to correspond to a

//        page mapped to something [code/data/bss/...])

//	\return the exception (generally the NO_EXCEPTION constant)

*/

ExceptionType PageFaultManager::PageFault(uint32_t virtualPage)

{
    

    auto translationTable = g_machine->mmu->translationTable;


    auto oldInt = g_machine->interrupt->SetStatus(INTERRUPTS_OFF);
    while (translationTable->getBitIo(virtualPage))
    {
        g_machine->interrupt->SetStatus(oldInt);
        g_current_thread->Yield();
        oldInt = g_machine->interrupt->SetStatus(INTERRUPTS_OFF);
    }
    translationTable->setBitIo(virtualPage);
    g_machine->interrupt->SetStatus(oldInt);

    auto inSwap = translationTable->getBitSwap(virtualPage);
    int diskAddr = translationTable->getAddrDisk(virtualPage);
    int np = g_physical_mem_manager->AddPhysicalToVirtualMapping(g_current_thread->GetProcessOwner()->addrspace, virtualPage);
    g_machine->mmu->translationTable->setPhysicalPage(virtualPage, np);
    

    if (inSwap) {
        // Page load from the swap
        while (diskAddr == -1) {
            g_current_thread->Yield();
            diskAddr = translationTable->getAddrDisk(virtualPage);
        }
        char buff[g_cfg->PageSize];
        g_swap_manager->GetPageSwap(diskAddr, buff);
        memcpy(&(g_machine->mainMemory[translationTable->getPhysicalPage(virtualPage) * g_cfg->PageSize]), buff, g_cfg->PageSize);
        translationTable->clearBitSwap(virtualPage);
        g_swap_manager->ReleasePageSwap(diskAddr);
    } else if ((!inSwap) && (diskAddr == -1)) {
        // anonymous page
        bzero(&(g_machine->mainMemory[translationTable->getPhysicalPage(virtualPage) * g_cfg->PageSize]), g_cfg->PageSize);

    } else {
        // load from the exec file
        auto exec_file = g_current_thread->GetProcessOwner()->exec_file;
        exec_file->ReadAt((char *)&(g_machine->mainMemory[translationTable->getPhysicalPage(virtualPage) * g_cfg->PageSize]), g_cfg->PageSize, diskAddr);
    }
    translationTable->clearBitIo(virtualPage);

    g_machine->mmu->translationTable->setBitValid(virtualPage);

    return NO_EXCEPTION;
}
