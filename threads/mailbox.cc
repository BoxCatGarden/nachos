/* mailbox.cc
 * Routines to manage mail transport.
 * 
 * A producer/comsumer problem, where the sender write a
 * new mail into an empty slot in the mailbox and the
 * reciever read a mail from the mailbox, the
 * corresponding slot reset empty.
-----------------------------------------------------------*/

#include "copyright.h"
#include "thread.h"
#include "synch.h"
#include "sysdep.h"

ThreadMailbox::ThreadMailbox(int size) : boxSize(size)
{
    waitReadList = new List<Thread *>;
    waitWrite = new Semaphore("Mailbox-waitWrite", size);
    box = new MailSlot[size];
    for (int i = 0; i < size; ++i)
    { //clear all slots
        box[i].size = 0;
    }
}

/*---------------------------------------------------------
 * no thread should wait the mailbox that is to be
 * destroied
---------------------------------------------------------*/
ThreadMailbox::~ThreadMailbox()
{
    delete waitReadList;
    delete waitWrite;
    delete[] box;
}

/*---------------------------------------------------------
 * send a mail into the mail box, blocking
 * 
 * recId: the recieving thread id. If the id is invalid,
 * the mail will be thrown away.
 * size: if size < 1, msg will be assumed to be a string
 * thus the size will equal the length of msg added 1.
---------------------------------------------------------*/
bool ThreadMailbox::Send(int recId, const char *msg, char size)
{
    if (size < 1)
        size = min(strlen(msg) + 1, TEXT_SIZE);

    Interrupt *interrupt = kernel->interrupt;
    IntStatus oldLevel = interrupt->SetLevel(IntOff); //turn off the interrupt

    waitWrite->P(); //declare the occupying of an empty slot
    if (!Thread::IsValidId(recId))
    { //invalid thread id
        waitWrite->V(); //put back the unused slot
        interrupt->SetLevel(oldLevel);
        return FALSE;
    }
    for (int i = 0; i < boxSize; ++i)
    { //find the empty slot
        if (!box[i].size)
        { //size = 0, empty slot
            WriteMail(i, recId, msg, size);
            interrupt->SetLevel(oldLevel);
            return TRUE;
        }
    }
}

/*---------------------------------------------------------
 * send a mail into the mail box, non-blocking
 * 
 * recId: the recieving thread id. If the id is invalid,
 * the mail will be thrown away.
 * size: if size < 1, msg will be assumed to be a string
 * thus the size will equal the length of msg added 1.
---------------------------------------------------------*/
bool ThreadMailbox::Put(int recId, const char *msg, char size)
{
    if (size < 1)
        size = min(strlen(msg) + 1, TEXT_SIZE);

    Interrupt *interrupt = kernel->interrupt;
    IntStatus oldLevel = interrupt->SetLevel(IntOff); //turn off the interrupt

    if (!Thread::IsValidId(recId))
    { //invalid thread id
        interrupt->SetLevel(oldLevel);
        return FALSE;
    }
    for (int i = 0; i < boxSize; ++i)
    {
        if (!box[i].size)
        { //size = 0, empty slot
            waitWrite->P(); //take one
            WriteMail(i, recId, msg, size);
            interrupt->SetLevel(oldLevel);
            return TRUE;
        }
    }
    interrupt->SetLevel(oldLevel);
    return FALSE;
}

/*---------------------------------------------------------
 * get a mail from the box, blocking
---------------------------------------------------------*/
void ThreadMailbox::Get(Mail *outMail)
{
    int id = kernel->currentThread->GetId();
    Interrupt *interrupt = kernel->interrupt;
    IntStatus oldLevel = interrupt->SetLevel(IntOff); //turn off the interrupt
    while (TRUE)
    {
        for (int i = 0; i < boxSize; ++i)
        {
            if (box[i].size && box[i].recieverId == id)
            {
                ReadMail(i, outMail);
                interrupt->SetLevel(oldLevel);
                return;
            }
        }
        WaitMail();
    }
}

/*---------------------------------------------------------
 * get a mail from the box, non-blocking
---------------------------------------------------------*/
bool ThreadMailbox::Pick(Mail *outMail)
{
    int id = kernel->currentThread->GetId();
    Interrupt *interrupt = kernel->interrupt;
    IntStatus oldLevel = interrupt->SetLevel(IntOff); //turn off the interrupt
    for (int i = 0; i < boxSize; ++i)
    {
        if (box[i].size && box[i].recieverId == id)
        {
            ReadMail(i, outMail);
            interrupt->SetLevel(oldLevel);
            return TRUE;
        }
    }
    interrupt->SetLevel(oldLevel);
    return FALSE; //instead of waiting, just return
}

/*---------------------------------------------------------
 * clear all the mails sent to the current thread
 * 
 * Note: a thread should call this when it is finishing
---------------------------------------------------------*/
void ThreadMailbox::Clear()
{
    int id = kernel->currentThread->GetId();
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    for (int i = 0; i < boxSize; ++i) {
        if (box[i].size && box[i].recieverId == id)
        {
            box[i].size = 0; //reset empty
            waitWrite->V();  //add one
        }
    }
    kernel->interrupt->SetLevel(oldLevel);
}

/*---------------------------------------------------------
 * awake the thread in waitReadList of which the id is id
---------------------------------------------------------*/
void ThreadMailbox::AwakeReader(int id)
{
    ListIterator<Thread *> it(waitReadList);
    while (!it.IsDone())
    {
        if (it.Item()->GetId() == id)
        {
            kernel->scheduler->ReadyToRun(it.Item());
            waitReadList->Remove(it.Item());
            return;
        }
        it.Next();
    }
}

/*---------------------------------------------------------
 * put the current thread into waitReadList for mail
---------------------------------------------------------*/
void ThreadMailbox::WaitMail() {
    waitReadList->Append(kernel->currentThread);
    kernel->currentThread->Sleep(FALSE);
}

/*---------------------------------------------------------
 * write a mail into slot i
---------------------------------------------------------*/
void ThreadMailbox::WriteMail(int slot, int recId, const char *msg, char size)
{
    box[slot].senderId = kernel->currentThread->GetId();
    box[slot].recieverId = recId;
    box[slot].size = size;
    memcpy(box[slot].text, msg, size);
    AwakeReader(recId);
}

/*---------------------------------------------------------
 * read a mail from slot i
---------------------------------------------------------*/
void ThreadMailbox::ReadMail(int slot, Mail *outMail)
{
    outMail->senderId = box[slot].senderId;
    outMail->size = box[slot].size;
    memcpy(outMail->text, box[slot].text, box[slot].size);
    box[slot].size = 0; //reset empty
    waitWrite->V();     //add one
}
