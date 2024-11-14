#include <stdio.h>
#include <math.h> 
#include <time.h>    
#include <limits.h>
#include <ctype.h>     
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "vm_dbg.h"

#define NOPS (16)

#define OPC(i) ((i)>>12)
#define DR(i) (((i)>>9)&0x7)
#define SR1(i) (((i)>>6)&0x7)
#define SR2(i) ((i)&0x7)
#define FIMM(i) ((i>>5)&01)
#define IMM(i) ((i)&0x1F)
#define SEXTIMM(i) sext(IMM(i),5)
#define FCND(i) (((i)>>9)&0x7)
#define POFF(i) sext((i)&0x3F, 6)
#define POFF9(i) sext((i)&0x1FF, 9)
#define POFF11(i) sext((i)&0x7FF, 11)
#define FL(i) (((i)>>11)&1)
#define BR(i) (((i)>>6)&0x7)
#define TRP(i) ((i)&0xFF)   

//New OS declarations
//  OS Bookkeeping constants 
#define OS_MEM_SIZE 4096    // OS Region size. At the same time, constant free-list starting header 

#define Cur_Proc_ID 0       // id of the current process
#define Proc_Count 1        // total number of processes, including ones that finished executing.
#define OS_STATUS 2         // Bit 0 shows whether the PCB list is full or not

//  Process list and PCB related constants
#define PCB_SIZE 6  // Number of fields in a PCB

#define PID_PCB 0   // holds the pid for a process
#define PC_PCB 1    // value of the program counter for the process
#define BSC_PCB 2   // base value of code section for the process
#define BDC_PCB 3   // bound value of code section for the process
#define BSH_PCB 4   // value of heap section for the process
#define BDH_PCB 5   // holds the bound value of heap section for the process

#define CODE_SIZE 4096
#define HEAP_INIT_SIZE 4096
//New OS declarations


bool running = true;

typedef void (*op_ex_f)(uint16_t i);
typedef void (*trp_ex_f)();

enum { trp_offset = 0x20 };
enum regist { R0 = 0, R1, R2, R3, R4, R5, R6, R7, RPC, RCND, RBSC, RBDC, RBSH, RBDH, RCNT };
enum flags { FP = 1 << 0, FZ = 1 << 1, FN = 1 << 2 };

uint16_t mem[UINT16_MAX] = {0};
uint16_t reg[RCNT] = {0};
uint16_t PC_START = 0x3000;

void initOS();
int createProc(char *fname, char* hname);
void loadProc(uint16_t pid);
static inline void tyld();
uint16_t allocMem(uint16_t size);
int freeMem(uint16_t ptr);
static inline uint16_t mr(uint16_t address);
static inline void mw(uint16_t address, uint16_t val);
static inline void tbrk();
static inline void thalt();
static inline void trap(uint16_t i);

static inline uint16_t sext(uint16_t n, int b) { return ((n>>(b-1))&1) ? (n|(0xFFFF << b)) : n; }
static inline void uf(enum regist r) {
    if (reg[r]==0) reg[RCND] = FZ;
    else if (reg[r]>>15) reg[RCND] = FN;
    else reg[RCND] = FP;
}
static inline void add(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] + (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void and(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] & (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void ldi(uint16_t i)  { reg[DR(i)] = mr(mr(reg[RPC]+POFF9(i))); uf(DR(i)); }
static inline void not(uint16_t i)  { reg[DR(i)]=~reg[SR1(i)]; uf(DR(i)); }
static inline void br(uint16_t i)   { if (reg[RCND] & FCND(i)) { reg[RPC] += POFF9(i); } }
static inline void jsr(uint16_t i)  { reg[R7] = reg[RPC]; reg[RPC] = (FL(i)) ? reg[RPC] + POFF11(i) : reg[BR(i)]; }
static inline void jmp(uint16_t i)  { reg[RPC] = reg[BR(i)]; }
static inline void ld(uint16_t i)   { reg[DR(i)] = mr(reg[RPC] + POFF9(i)); uf(DR(i)); }
static inline void ldr(uint16_t i)  { reg[DR(i)] = mr(reg[SR1(i)] + POFF(i)); uf(DR(i)); }
static inline void lea(uint16_t i)  { reg[DR(i)] =reg[RPC] + POFF9(i); uf(DR(i)); }
static inline void st(uint16_t i)   { mw(reg[RPC] + POFF9(i), reg[DR(i)]); }
static inline void sti(uint16_t i)  { mw(mr(reg[RPC] + POFF9(i)), reg[DR(i)]); }
static inline void str(uint16_t i)  { mw(reg[SR1(i)] + POFF(i), reg[DR(i)]); }
static inline void rti(uint16_t i) {} // unused
static inline void res(uint16_t i) {} // unused
static inline void tgetc() { reg[R0] = getchar(); }
static inline void tout() { fprintf(stdout, "%c", (char)reg[R0]); }
static inline void tputs() {
    uint16_t *p = mem + reg[R0];
    while(*p) {
        fprintf(stdout, "%c", (char)*p);
        p++;
    }
}
static inline void tin() { reg[R0] = getchar(); fprintf(stdout, "%c", reg[R0]); }
static inline void tputsp() { /* Not Implemented */ }

static inline void tinu16() { fscanf(stdin, "%hu", &reg[R0]); }
static inline void toutu16() { fprintf(stdout, "%hu\n", reg[R0]); }


trp_ex_f trp_ex[10] = { tgetc, tout, tputs, tin, tputsp, thalt, tinu16, toutu16, tyld, tbrk };
static inline void trap(uint16_t i) { trp_ex[TRP(i)-trp_offset](); }
op_ex_f op_ex[NOPS] = { /*0*/ br, add, ld, st, jsr, and, ldr, str, rti, not, ldi, sti, jmp, res, lea, trap };

void ld_img(char *fname, uint16_t offset, uint16_t size) {
    FILE *in = fopen(fname, "rb");
    if (NULL==in) {
        fprintf(stderr, "Cannot open file %s.\n", fname);
        exit(1);    
    }
    uint16_t *p = mem + offset;
    fread(p, sizeof(uint16_t), (size), in);
    fclose(in);
}

void run(char* code, char* heap) {

    while(running) {
        uint16_t i = mr(reg[RPC]++);
        op_ex[OPC(i)](i);
    }
}


// YOUR CODE STARTS HERE

// Helper functions


// Function to simulates a delay (for non-functional purposes)
void simulateDelay(int seconds) {
    clock_t start_time = clock();
    while (clock() < start_time + seconds * CLOCKS_PER_SEC);
}

// Function to convert a string to uppercase
void convertToUpper(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = toupper(str[i]);
    }
}

// Return the size of the free memory block at a given header address
uint16_t getFreeSize(uint16_t header) {
    return mem[header];
}

// Return the next free memory block's header address from the given header address
uint16_t getFreeNext(uint16_t header) {
    return mem[header + 1];
}

// Find and return the header address of the previous free memory block of the given header address
uint16_t getFreePrevHeader(uint16_t header) {
    uint16_t prevHeader = OS_MEM_SIZE; // Start from the beginning of the free list

    // Traverse the free list until finding the block just before the header
    while (getFreeNext(prevHeader) != 0 && getFreeNext(prevHeader) < header) {
        prevHeader = getFreeNext(prevHeader);
    }

    return prevHeader; // Return the header address
}

// Find and return the header address of the next free memory block of the given header address
uint16_t getFreeNextHeader(uint16_t header) {
    uint16_t prevHeader = getFreePrevHeader(header); // Find the previous header
    uint16_t nextHeader = getFreeNext(prevHeader); // Get the next header from the previous header

    // If the next header is the same as the current header, get the next header from it
    if (nextHeader == header) {
        nextHeader = getFreeNext(nextHeader);
    }

    return nextHeader; // Return the next free block's header address
}

// Set the memory block header
void setHeader(uint16_t headerAddr, uint16_t size, uint16_t other) {
    mem[headerAddr] = size; // Set the size of the memory block
    mem[headerAddr + 1] = other; // Set the additional information (e.g., next free block or magic number)
}

// Check if two memory blocks are contiguous
bool isContiguous(uint16_t header1, uint16_t header2) {
    return (header1 + getFreeSize(header1) + 2 == header2);
}

// Coalesce the current block with the previous block if they are contiguous
void coalescePrev(uint16_t header) {
    uint16_t prevHeader = getFreePrevHeader(header); // Find the previous block's header

    // If the previous block is contiguous with the current block
    if (isContiguous(prevHeader, header)) {
        // Merge the two blocks by updating the size and next pointer of the previous block
        setHeader(prevHeader, getFreeSize(prevHeader) + getFreeSize(header) + 2, getFreeNext(header));
        setHeader(header, 0, 0); // Clear the current block's header
    }
}

// Coalesce the current block with the next block if they are contiguous
void coalesceNext(uint16_t header) {
    uint16_t nextHeader = getFreeNextHeader(header); // Find the next block's header

    // If the current block is contiguous with the next block
    if (isContiguous(header, nextHeader)) {
        // Merge the two blocks by updating the size and next pointer of the current block
        setHeader(header, getFreeSize(header) + getFreeSize(nextHeader) + 2, getFreeNext(nextHeader));
        setHeader(nextHeader, 0, 0); // Clear the next block's header
    }
}

// Coalesce the current block with both the previous and next blocks if they are contiguous
void coalesce(uint16_t header) {
    coalesceNext(header); // Attempt to merge with the next block 
    coalescePrev(header); // Attempt to merge with the previous block
}

// Return the address of the process control block (PCB) for the process with the provided pid
uint16_t computePcbAddress(uint16_t pid) {
    return 12 + pid * PCB_SIZE; // Calculate the PCB address based on the process ID and PCB size
}

// Save the current process's registers to its PCB
void saveProcessState() {
    uint16_t pid = mem[Cur_Proc_ID]; // Get the current process ID
    uint16_t pcbAddress = computePcbAddress(pid); // Calculate the PCB address
    simulateDelay(10)

    // Save the registers to the PCB
    mem[pcbAddress + PC_PCB] = reg[RPC];
    mem[pcbAddress + BSC_PCB] = reg[RBSC];
    mem[pcbAddress + BDC_PCB] = reg[RBDC];
    mem[pcbAddress + BSH_PCB] = reg[RBSH];
    mem[pcbAddress + BDH_PCB] = reg[RBDH];
}

// Find and return the process ID of the next runnable process
uint16_t findNextRunnableProcess() {
    uint16_t pid = mem[Cur_Proc_ID]; // Get the current process ID
    uint16_t nextProcID = (pid + 1) % mem[Proc_Count]; // Calculate the next process ID by making sure to loop back

    // Iterate through the PCBs to find the next runnable process
    while (mem[12 + nextProcID * PCB_SIZE] == 0xFFFF) {
        nextProcID = (nextProcID + 1) % mem[Proc_Count];
        if (nextProcID == mem[Cur_Proc_ID]) {
            // No other runnable process, continue with the current process    
            break;
        }
    }

    return nextProcID; // Return the next runnable process ID
}

// Check if an address is valid within a segment, and if so, set the base and bound values for the segment
bool isAddrValid(uint16_t addr, uint16_t *base, uint16_t *bound) {
    uint16_t segment = addr >> 12; // Extract the segment part of the address (first 4 bits)
    uint16_t offset = addr & 0x0FFF; // Extract the offset part of the address (last 12 bits)

    // Check if the segment is valid (code or heap segment)
    if ((segment & 0xC) == 0x0 && (segment != 0x3)) {
        printf("Segmentation Fault.\n");
        return false;
    }

    // Set the base and bound values based on the segment type
    if (segment == 0x3) { // Code segment
        *base = reg[RBSC];
        *bound = reg[RBDC];
    } 
    else if (segment == 0x4) { // Heap segment
        *base = reg[RBSH];
        *bound = reg[RBDH];
    } 
    else {
        return false;
    }

    // Perform bound check to ensure the offset is within the segment's bound
    if (offset > *bound) {
        if (segment == 0x4) {
            printf("Segmentation Fault Inside Heap Segment.\n");
        } else if (segment == 0x3) {
            printf("Segmentation Fault Inside Code Segment.\n");
        }
        return false;
    }

    return true;
}

// Initialize the operating system's memory management structures
void initOS() {
    mem[Cur_Proc_ID] = 0xFFFF; // Set the current process ID to an invalid value
    mem[Proc_Count] = 0; // Initialize the process count to zero
    mem[OS_MEM_SIZE] = 0xEFFE; // Set the size of the operating system's memory region
}

// Create a new process
int createProc(char *fname, char* hname) {

    // Check if the process count has reached the maximum allowed processes
    if (mem[Proc_Count] == 340) {
        mem[OS_STATUS] = 0xF000; // Set the OS status to indicate that memory is full
    }

    // Check if the OS status indicates that memory is full
    if ((mem[OS_STATUS] & 0xF000) == 0xF000) {
        printf("The OS memory region is full. Cannot create a new PCB.\n");
        return 1; 
    }

    uint16_t pid = mem[Proc_Count]; // Get the new process ID
    mem[Proc_Count]++; // Increment the process count
    uint16_t pcbAddress = computePcbAddress(pid); // Calculate the PCB address

    // Allocate memory for the code segment
    uint16_t codeAddress = allocMem(4096); // Allocate 4KB for code
    if (codeAddress == 0) {
        printf("Cannot create code segment.\n");
        return 0; // Return failure
    }
    ld_img((char *)fname, codeAddress, 4096); // Load code from file
    mem[pcbAddress + BSC_PCB] = codeAddress; // Set the base address of the code segment
    mem[pcbAddress + BDC_PCB] = CODE_SIZE; // Set the bound (size) of the code segment

    // Allocate memory for the heap segment
    uint16_t heap_address = allocMem(4096); // Allocate 4KB for heap
    if (heap_address == 0) {
        printf("Cannot create heap segment.\n");
        return 0; // Return failure
    }
    ld_img((char *)hname, heap_address, 4096); // Load heap from file
    mem[pcbAddress + BSH_PCB] = heap_address; // Set the base address of the heap segment
    mem[pcbAddress + BDH_PCB] = HEAP_INIT_SIZE; // Set the bound (size) of the heap segment

    return 0; 
}

// Load process into CPU registers
void loadProc(uint16_t pid) {
    uint16_t pcbAddress = computePcbAddress(pid); // Calculate the PCB address
    mem[Cur_Proc_ID] = pid; // Set the current process ID
    // Load the registers from the PCB
    reg[RPC] = mem[pcbAddress + PC_PCB];
    reg[RBSC] = mem[pcbAddress + BSC_PCB];
    reg[RBDC] = mem[pcbAddress + BDC_PCB];
    reg[RBSH] = mem[pcbAddress + BSH_PCB];
    reg[RBDH] = mem[pcbAddress + BDH_PCB];
}

// Free allocated memory block
int freeMem(uint16_t ptr) {
    // Check if the address is within valid range
    if (ptr < OS_MEM_SIZE || ptr > 65535) {
        return 1; // 
    }
    
    uint16_t addrHeader = ptr - 2; // Calculate the header address of the given address

    uint16_t chunkSize = mem[addrHeader]; // Get the size of the chunk
    uint16_t magicNumber = mem[addrHeader + 1]; // Get the magic number

    // Check if the magic number is valid
    if (magicNumber != 42) {
        return 1;
    }

    uint16_t prevHeader = getFreePrevHeader(addrHeader); // Get the previous free block's header
    uint16_t nextHeader = getFreeNextHeader(addrHeader); // Get the next free block's header

    // Update the free list to include the freed block
    mem[addrHeader + 1] = nextHeader;
    mem[prevHeader + 1] = addrHeader;

    coalesce(addrHeader); // Attempt to coalesce the freed block with adjacent free blocks
    
    return 0; // Return success
}

// Allocate memory block
uint16_t allocMem(uint16_t size) {
    uint16_t header = 4096; // Start from the beginning of the free list
    uint16_t freeSize;

    // Iterate through the free list to find a suitable free block
    while (header != 65535) {
        freeSize = getFreeSize(header);
        // Check if the current free block is large enough
        if (freeSize >= size + 2) {
            uint16_t newHeader = header + freeSize - size; // Calculate the new header address
            
            setHeader(newHeader, size, 42); // Set the new header with size and magic number

            // If there is remaining space, update the original free block's header
            if (remainingSpace > 0) {
                setHeader(header, remainingSpace, getFreeNext(header));
            }
            return newHeader + 2; // Return the address of the allocated memory block
        }
        header = getFreeNext(header); // Move to the next free block
    }
    return 1; 
}

// Implement tbrk system call
static inline void tbrk() {
    uint16_t pid = mem[Cur_Proc_ID]; // Get the current process ID
    uint16_t pcbAddress = computePcbAddress(pid); // Calculate the PCB address

    uint16_t oldSize = reg[RBDH]; // Get the old size of the heap
    uint16_t newSize = reg[R0]; // Get the new size of the heap from register R0
    int sizeDiff = newSize - oldSize; // Calculate the size difference

    uint16_t heapBase = reg[RBSH]; // Get the base address of the heap
    uint16_t heapHeader = heapBase - 2; // Calculate the header address of the heap

    uint16_t prevHeader = getFreePrevHeader(heapHeader); // Get the previous free block's header
    uint16_t nextHeader = getFreeNextHeader(heapHeader); // Get the next free block's header

    if (newSize == oldSize) {
        return; // If the new size is equal to old size, return without doing anything
    }

    // Check if we need to allocate more space and there is no next free block
    if (nextHeader == 0 && sizeDiff > 0) {
        printf("Cannot allocate more space for the heap of pid %d since we bumped into an allocated region.\n", pid);
        return;
    }
    
    uint16_t nextNextFree;

    // Clear the next free block if it exists
    if (nextHeader != 0) {
        nextNextFree = getFreeNext(nextHeader);
        setHeader(nextHeader, 0, 0);
        convertToUpper('s');
    }

    uint16_t newNextHeader;

    // If we need to expand the heap
    if (newSize < oldSize) {
        // Check if we can expand into the next free block
        if (isContiguous(heapHeader, nextHeader) && (getFreeSize(nextHeader) + 2 >= sizeDiff)) {
            // If the next free block is too large, create a new header for the remaining space
            if (getFreeSize(nextHeader) - sizeDiff > 2) {
                newNextHeader = nextHeader + sizeDiff;
                setHeader(prevHeader, getFreeSize(prevHeader), newNextHeader);
                setHeader(newNextHeader, getFreeSize(nextHeader) - sizeDiff, nextNextFree);
            }
            setHeader(heapHeader, newSize, 42); // Update the heap header with the new size
            mem[pcbAddress + BDH_PCB] = newSize; // Update the PCB with the new heap size
        } 
        else {
            // If we cannot expand the heap, print an error message
            if (getFreeSize(prevHeader) < sizeDiff) {
                printf("Cannot allocate more space for the heap of pid %d since total free space size here is not enough.\n", pid);
            }
            else {
                printf("Cannot allocate more space for the heap of pid %d since we bumped into an allocated region.\n", pid);
            }
            return;
        }
    } else {
        // If we need to shrink the heap
        if ((-sizeDiff) >= 2) {
            newNextHeader = heapHeader + oldSize + 2 + sizeDiff;
            setHeader(newNextHeader, -sizeDiff - 2, 42);
            setHeader(heapHeader, newSize, 42);
            freeMem(newNextHeader + 2);
            prevHeader = getFreePrevHeader(newNextHeader);
        }
        mem[pcbAddress + BDH_PCB] = newSize; // Update the PCB with the new heap size
    }
}

// System call implementations

// Implement tyld system call to yield the processor
static inline void tyld() {
    uint16_t nextProcID = findNextRunnableProcess(); // Find the next runnable process
    uint16_t pid = mem[Cur_Proc_ID]; // Get the current process ID

    // If the current process is not the same as the next process
    if (pid != nextProcID) {
        saveProcessState(); // Save the current process state
        printf("We are switching from process %d to %d.\n", pid, nextProcID);
        loadProc(nextProcID); // Load the next process
    }
}

// Implement thalt system call to halt the process
static inline void thalt() {
    uint16_t pid = mem[Cur_Proc_ID]; // Get the current process ID
    uint16_t pcbAddress = computePcbAddress(pid); // Calculate the PCB address    

    // Free the code and heap segments of the terminating process
    uint16_t heapBase = mem[pcbAddress + BSH_PCB]; // Get the base address of the heap
    uint16_t codeBase = mem[pcbAddress + BSC_PCB]; // Get the base address of the code

    freeMem(heapBase); // Free the heap memory
    freeMem(codeBase); // Free the code memory

    uint16_t nextProcID = findNextRunnableProcess(); // Find the next runnable process


    // If the current process is the same as the next process
    if (pid == nextProcID) {
        // No other runnable process, halt the VM
        running = false;
    } 
    else {
        // Load the next process
        loadProc(nextProcID);
    }
} 

// Memory read method
uint16_t mr(uint16_t addr) {
    uint16_t base, bound;
    uint16_t offset = addr & 0x0FFF;

    if (isAddrValid(addr, &base, &bound))
        return mem[base + offset];

}

// Memory write method
void mw(uint16_t addr, uint16_t value) {
    uint16_t base, bound;
    uint16_t offset = addr & 0x0FFF;

    if (isAddrValid(addr, &base, &bound))
        mem[base + offset] = value;

}


// YOUR CODE ENDS HERE
