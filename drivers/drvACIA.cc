/* \file drvACIA.cc

   \brief Routines of the ACIA device driver

//

//      The ACIA is an asynchronous device (requests return

//      immediately, and an interrupt happens later on).

//      This is a layer on top of the ACIA.

//      Two working modes are to be implemented in assignment 2:

//      a Busy Waiting mode and an Interrupt mode. The Busy Waiting

//      mode implements a synchronous IO whereas IOs are asynchronous

//      IOs are implemented in the Interrupt mode (see the Nachos

//      roadmap for further details).

//

//  Copyright (c) 1999-2000 INSA de Rennes.

//  All rights reserved.

//  See copyright_insa.h for copyright notice and limitation

//  of liability and disclaimer of warranty provisions.

//

*/

/* Includes */

#include "drivers/drvACIA.h"

#include "kernel/synch.h"
#include "kernel/system.h"  // for the ACIA object
#include "machine/ACIA.h"

//-------------------------------------------------------------------------

// DriverACIA::DriverACIA()

/*! Constructor.

  Initialize the ACIA driver.

  In the ACIA Interrupt mode,

  initialize the reception index and semaphores and allow

  reception interrupts.

  In the ACIA Busy Waiting mode, simply inittialize the ACIA

  working mode and create the semaphore.

  */

//-------------------------------------------------------------------------

DriverACIA::DriverACIA()

{
    
    
#ifndef ETUDIANTS_TP
    printf("**** Warning: contructor of the ACIA driver not implemented yet\n");

    exit(-1);
#endif
#ifdef ETUDIANTS_TP
    
    
    if (g_cfg->ACIA== ACIA_BUSY_WAITING) { 
        
        this->send_sema = new Semaphore("sem_send", 1);
        this->receive_sema = new Semaphore("sem_receive", 1);
        g_machine->acia->SetWorkingMode(BUSY_WAITING);
    } else if (g_cfg->ACIA == ACIA_INTERRUPT) {
        this->send_sema = new Semaphore("sem_send", 0);
        this->receive_sema = new Semaphore("sem_receive", 0);
        this->ind_rec = 0;
        this->ind_send = 0; // TODO
    }
    printf("ici\n");

#endif

}

//-------------------------------------------------------------------------

// DriverACIA::TtySend(char* buff)

/*! Routine to send a message through the ACIA (Busy Waiting or Interrupt mode)

  */

//-------------------------------------------------------------------------

int DriverACIA::TtySend(char* buff)

{
#ifndef ETUDIANTS_TP
    printf("**** Warning: method Tty_Send of the ACIA driver not implemented yet\n");

    exit(-1);

    return 0;
#endif
#ifdef ETUDIANTS_TP
    ASSERT(this->send_sema != nullptr);
    
    if (g_cfg->ACIA == ACIA_BUSY_WAITING) {
        printf("ICI\n");
        this->send_sema->P();
        int i = 0;
        
        while (i < BUFFER_SIZE && buff[i] != '\0') {
            while (g_machine->acia->GetOutputStateReg() != EMPTY) {
            }
            g_machine->acia->PutChar(buff[i]);
            i++;
        }  // one more loop to send \0
        if (i < BUFFER_SIZE) {
            while (g_machine->acia->GetOutputStateReg() != EMPTY) {
            }
            g_machine->acia->PutChar(buff[i]);
            i++;
        }

        this->send_sema->V();

        return i;
    } else  if (g_cfg->ACIA == ACIA_INTERRUPT) {
        printf("**** Warning: method TtySend of the ACIA driver for interruptions not implemented yet\n");
        int i=0;
        printf("test0\n");
        while( i<BUFFER_SIZE && buff[i] != 0){
            printf("i: %d p:%p\n",i,this->send_buffer);
            this->send_buffer[i]=buff[i];
            i++;
            
        }
        printf("test2fin\n");
        g_machine->acia->PutChar(this->send_buffer[this->ind_send]);
        
        this->send_sema->P();   
        return i;
        
    }
#endif
}

//-------------------------------------------------------------------------

// DriverACIA::TtyReceive(char* buff,int length)

/*! Routine to reveive a message through the ACIA

//  (Busy Waiting and Interrupt mode).

  */

//-------------------------------------------------------------------------

int DriverACIA::TtyReceive(char* buff, int lg)

{
#ifndef ETUDIANTS_TP
    printf("**** Warning: method Tty_Receive of the ACIA driver not implemented yet\n");

    exit(-1);

    return 0;
#endif
#ifdef ETUDIANTS_TP
    if (g_cfg->ACIA == ACIA_BUSY_WAITING) {
        printf("ICI\n");
        this->receive_sema->P();
        int i = -1;
        do {
            i++;
            while (g_machine->acia->GetInputStateReg() != FULL) {
            }
            buff[i] = g_machine->acia->GetChar();

        } while (buff[i] != '\0' && i < BUFFER_SIZE - 1 && i < lg - 1);
        this->receive_sema->V();
        return i;
    } else  if (g_cfg->ACIA == ACIA_INTERRUPT) { 
        printf("**** Warning: method TtyReceive of the ACIA driver for interruptions not implemented yet\n");
        int i=0;

        printf("iciOO\n");
        this->receive_sema->P();
        while( i<BUFFER_SIZE && this->receive_buffer[i] != 0){
            printf("i:%d c:%c\n",i,this->receive_buffer[i]);
            buff[i]= this->receive_buffer[i];
            i++;
        }
        printf("iciOO\n");
        this->send_sema->V();
        return i;
    }
#endif
}

//-------------------------------------------------------------------------

// DriverACIA::InterruptSend()

/*! Emission interrupt handler.

  Used in the ACIA Interrupt mode only.

  Detects when it's the end of the message (if so, releases the send_sema semaphore), else sends the next character according to index ind_send.

  */

//-------------------------------------------------------------------------

void DriverACIA::InterruptSend()

{
#ifndef ETUDIANTS_TP
    printf("**** Warning: send interrupt handler not implemented yet\n");

    exit(-1);
#endif
#ifdef ETUDIANTS_TP
    printf("**** Warning: send interrupt handler not implemented yet bonjour\n");
    printf("ind : %d\n",this->ind_rec);
    if(this->ind_send< BUFFER_SIZE)
    {   
        printf("test1\n");
        this->ind_send++;
        g_machine->acia->PutChar(this->send_buffer[this->ind_send]);
        
    }else {
        printf("test2 ok fin \n");
        this->send_sema->V();

    }
    
#endif
}

//-------------------------------------------------------------------------

// DriverACIA::Interrupt_receive()

/*! Reception interrupt handler.

  Used in the ACIA Interrupt mode only. Reveices a character through the ACIA.

  Releases the receive_sema semaphore and disables reception

  interrupts when the last character of the message is received

  (character '\0').

  */

//-------------------------------------------------------------------------

void DriverACIA::InterruptReceive()

{
#ifndef ETUDIANTS_TP
    printf("**** Warning: receive interrupt handler not implemented yet\n");

    exit(-1);
#endif
#ifdef ETUDIANTS_TP
    printf("**** Warning: receive interrupt handler not implemented yet\n");
    printf("odkdok\n");
    receive_buffer[ind_rec] = g_machine->acia->GetChar();
    if((receive_buffer[ind_rec] == '\0') || (ind_rec+1 == BUFFER_SIZE)){
        receive_sema->V();
        g_machine->acia->SetWorkingMode(SEND_INTERRUPT);
    }else{
        ind_rec++;
    }

    
#endif
}
