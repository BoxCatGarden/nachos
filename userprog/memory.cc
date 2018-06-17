/* memory.cc
 * Memory management */

#include "copyright.h"
#include "main.h"
#include "memory.h"

const int NumVirtPages = NumPhysPages << 1;

static const unsigned char bit[8]  = { 128,  64,  32,  16,  8,  4,  2,  1};
static const unsigned char rbit[8] = {-129, -65, -33, -17, -9, -5, -3, -2};

Memory::Memory(char* virtualMemFile) {
    int i;

    virtualMem = kernel->fileSystem->Open(virtualMemFile);

    //initialize the allocation of memory and virtual memory, and virtual page mapping
    memUsage = new unsigned char[NumPhysPages>>3];
    for (i = 0; i < (NumPhysPages >> 3); ++i)
        memUsage[i] = 0;

    virUsage = new unsigned char[NumPhysPages>>2];
    for (i = 0; i < (NumPhysPages >> 2); ++i)
        virUsage[i] = 0;

    pageTable = new TranslationEntry[NumVirtPages];
    for (i = 0; i < NumVirtPages; ++i) {
        TranslationEntry &entry = pageTable[i];
        entry.virtualPage = i;
        entry.physicalPage = 0;
        entry.valid = FALSE;
        entry.use = FALSE;
        entry.dirty = FALSE;
        entry.readOnly = FALSE;
    }
    kernel->machine->pageTable = pageTable; //mounting
}

Memory::~Memory() {
    delete[] memUsage;
    delete[] virUsage;
    delete[] pageTable;

    delete virtualMem;
}

void Memory::Alloc(int pageFrame) {
    memUsage[pageFrame>>3] |= bit[pageFrame&7];
}

void Memory::Free(int pageFrame) {
    memUsage[pageFrame>>3] &= rbit[pageFrame&7];
}

/* Load process pages into physical memory
 * Return the number of successful loading 
 * Replacement, if necessary, happens here */
int Memory::Load(int startPrcPage, int *prcPT, int num) {
    int i;
    for (i = 0; i < num && i < NumPhysPages; ++i) {
        int j = FindFreeFrame();
        if (j == NumPhysPages) {
            j = ReplaceFrame();
        }
        LoadIntoFrame(j, prcPT[startPrcPage + i]);
    }
    return i;
}

int Memory::FindFreeFrame() {
    int i;
    for (i = 0; i < NumPhysPages && (memUsage[i>>3]&bit[i&7]); ++i);
    return i;
}

int Memory::ReplaceFrame() {
    //clock with modifying bit
    int i;
    while (1)
    {
        for (i = 0; i < NumVirtPages; ++i)
        {
            TranslationEntry &entry = pageTable[i];
            if (entry.valid && !entry.use && !entry.dirty)
            {
                entry.valid = FALSE;
                return entry.physicalPage;
            }
        }

        for (i = 0; i < NumVirtPages; ++i)
        {
            TranslationEntry &entry = pageTable[i];
            if (entry.valid)
            {
                if (!entry.use)
                {
                    /* As we set the valid bit FALSE without Free()
                     *   the corresponding frame, the frame won't
                     *   be detected by neither FindFreeFrame() nor
                     *   ReplaceFrame() so that it's unavailable for
                     *   other processes, promising the safe I/O which
                     *   might involve process switching */
                    entry.valid = FALSE;
                    WriteBack(entry.physicalPage, i);
                    return entry.physicalPage;
                }
                else
                    entry.use = FALSE;
            }
        }
    }
}

void Memory::LoadIntoFrame(int frame, int page)
{
    Alloc(frame);
    virtualMem->ReadAt(&(kernel->machine->mainMemory[frame * PageSize]),
                       PageSize, page * PageSize);
    pageTable[page].physicalPage = frame;
    pageTable[page].use = FALSE;
    pageTable[page].dirty = FALSE;
    pageTable[page].valid = TRUE;
}

void Memory::WriteBack(int frame, int page)
{
    virtualMem->WriteAt(&(kernel->machine->mainMemory[frame * PageSize]),
                        PageSize, page * PageSize);
    pageTable[page].dirty = FALSE;
}

/* Allocate virtual memery sized [pageNum] pages
 * Return the process page table,
 *   a simple array of int, mapping from
 *   process page to virtual page
 * If [pageNum] is more than the available pages
 *   in virtual memory, return NULL */
int *Memory::StartVirLoad(int pageNum) {
    int i;
    int j = 0;
    int *prcPT = new int[pageNum];
    ASSERT(prcPT != NULL);
    for (i = 0; i < NumVirtPages && j < pageNum; ++i) {
        if (!(virUsage[i>>3]&bit[i&7])) {
            prcPT[j++] = i; //mapping
        }
    }

    if (j == pageNum) { //enough
        for (j = 0; j < pageNum; ++j) {
            virUsage[prcPT[j]>>3] |= bit[prcPT[j]&7]; //opcupying
        }
        return prcPT;
    } else {
        delete[] prcPT;
        return NULL;
    }
}

static void ReadIntoBuf(char* buf, OpenFile* file, int size, int position) {
    ASSERT(size == file->ReadAt(buf, size, position));
}

/* Load files into allocated virtual memory
 * Read [numBytes] bytes from [file] begining at
 *   [position], and write them into virtual memory,
 *   with respect to [prcPT], begining at
 *   page [prcPage] with offset [offset]
 * [prcPage] will be updated for the next loading
 * [offset] will be updated for the next loading
 * Note: the file pointer will move forward
 *   while reading the file */
void Memory::VirLoad(int *prcPT, int prcAddr,
        OpenFile *file, int numBytes, int position) {
    int prcPage = prcAddr/PageSize;
    int offset = prcAddr%PageSize;
    char *page4 = new char[PageSize<<2]; //buffer
    int i;
    int loaded;
    int size;

    //align offset
    size = offset + numBytes > PageSize ? PageSize-offset : numBytes;
    ReadIntoBuf(page4, file, size, position);
    WriteVirMem(page4, size, prcPT[prcPage]*PageSize+offset);
    offset = (offset + size) % PageSize;
    if (!offset) ++prcPage;
    loaded = size;

    //paged loading
    while (loaded + (PageSize<<2) <= numBytes) {
        ReadIntoBuf(page4, file, (PageSize<<2), position + loaded);
        for (i = 0; i < 4; ++i) {
            WriteVirMem(&page4[i*PageSize], PageSize, prcPT[prcPage++]*PageSize);
        }
        loaded += (PageSize<<2);
    }

    //last loading
    if (loaded < numBytes) {
        size = numBytes - loaded;
        ReadIntoBuf(page4, file, size, position + loaded);
        for (i = 0; i < size/PageSize; ++i) {
            WriteVirMem(&page4[i*PageSize], PageSize, prcPT[prcPage++]*PageSize);
            loaded += PageSize;
        }
        if (loaded < numBytes) {
            size = numBytes - loaded;
            WriteVirMem(&page4[i*PageSize], size, prcPT[prcPage]*PageSize);
            offset = size;
            loaded = numBytes;
        }
    }
}

void Memory::WriteVirMem(char *buf, int size, int position) {
    ASSERT(size == virtualMem->WriteAt(buf, size, position));
}

/* Release the virtual page w.r.t [prcPT] of length [length]
 * Will also delete the process page table */
void Memory::VirRelease(int *prcPT, int length) {
    int i;
    for (i = 0; i < length; ++i) {
        TranslationEntry &entry = pageTable[prcPT[i]];
        if (entry.valid) { //release page frame and reset the entry
            entry.valid = FALSE;
            Free(entry.physicalPage);
        }
        virUsage[prcPT[i]>>3] &= rbit[prcPT[i]&7]; //release page
    }
    delete[] prcPT;
}