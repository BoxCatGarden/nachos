/*
 * Memory management component.
*/

#ifndef MEM_H
#define MEM_H

#include "copyright.h"

extern const int NumVirtPages;

class TranslationEntry;
class OpenFile;

class Memory
{
    unsigned char *memUsage; //memory usage
    unsigned char *virUsage; //virtual memory usage

    /* Mapping from virtual memory to physical memory
     * Will be mounted at Machine::pageTable */
    TranslationEntry *pageTable;

    OpenFile *virtualMem;

    void Alloc(int pageFrame);
    void Free(int pageFrame);
    int FindFreeFrame();
    int ReplaceFrame();
    void LoadIntoFrame(int frame, int page);
    void WriteBack(int frame, int page);
    void WriteVirMem(char *buf, int size, int position);

  public:
    /* Create virtual memory with file [virtualMemFile]
     * Mount the page table at Machine::pageTable */
    Memory(char* virtualMemFile);
    ~Memory();

    // Load process pages into physical memory
    int Load(int startPrcPage, int *prcPT, int num = 1);

    // Allocate virtual memery sized [pageNum] pages
    int *StartVirLoad(int pageNum);

    // Load files into allocated virtual memory
    void VirLoad(int *prcPT, int prcAddr,
        OpenFile *file, int numBytes, int position);

    // Release the virtual page w.r.t [prcPT] of length [length]
    void VirRelease(int *prcPT, int length);
};

#endif