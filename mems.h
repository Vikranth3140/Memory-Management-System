/*All the main functions with respect to the MeMS are inplemented here
read the function discription for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions 
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNSTIONS AND THEIR FUNCTIONALITY
*/
// add other headers as required
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>


/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this 
macro to make the output of all system same and conduct a fair evaluation. 
*/
#define PAGE_SIZE 4096

/* Making the free list structure */

struct Segment {
    int type;  // 1 for process, 0 for hole
    int size;
    struct Segment* prev;
    struct Segment* next;
    struct Node* mainNode;
};

struct Node {
    int size;
    struct Node* prev;
    struct Node* next;
    struct Segment* segment;
};

struct MainChain {
    struct Node* head;
};

struct MainChain* mainChain;
size_t memsVirtualOffset = 0;

struct Node* createNode(int size) {
    struct Node* newNode = mmap(NULL, sizeof(struct Node), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (newNode == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    newNode->size = size;
    newNode->prev = mainChain->head;
    newNode->next = NULL;
    newNode->segment = NULL;
    return newNode;
}

void appendNode(int size) {
    struct Node* newNode = createNode(size);
    if (mainChain->head == NULL) {
        mainChain->head = newNode;
    } else {
        struct Node* current = mainChain->head;

        while (current->next) {
            current = current->next;
        }
        current->next = newNode;
        newNode->prev = current;
    }
}

void appendSegment(struct Node* node, int type, int size) {
    struct Segment* newSegment = mmap(NULL, sizeof(struct Segment), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (newSegment == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    newSegment->type = type;
    newSegment->mainNode = node;
    newSegment->prev = NULL;
    newSegment->next = NULL;
    newSegment->size = size;

    if (node->segment == NULL) {
        node->segment = newSegment;
    } else {
        struct Segment* current = node->segment;

        while (current->next) {
            current = current->next;
        }
        current->next = newSegment;
        newSegment->prev = current;
    }
}

/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_init(){
    mainChain = (struct MainChain*)mmap(NULL, sizeof(struct MainChain), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mainChain == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    mainChain->head = NULL;
}


/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish() {
    struct Node* currentNode = mainChain->head;
    while (currentNode) {
        struct Node* nextNode = currentNode->next;
        struct Segment* currentSegment = currentNode->segment;

        while (currentSegment) {
            struct Segment* nextSegment = currentSegment->next;
            munmap(currentSegment, sizeof(struct Segment));
            currentSegment = nextSegment;
        }
        munmap(currentNode, sizeof(struct Node));
        currentNode = nextNode;
    }
    munmap(mainChain, sizeof(struct MainChain));
}


/*
Allocates memory of the specified size by reusing a segment from the free list if 
a sufficiently large segment is available. 

Else, uses the mmap system call to allocate more memory on the heap and updates 
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/ 

void* mems_malloc(size_t size) {
    uintptr_t memsVirtualAddress = 0;
    size_t total_size = size * PAGE_SIZE;

    if (mainChain->head == NULL) {
        mainChain->head = createNode(total_size);
        appendSegment(mainChain->head, 0, total_size);
        memsVirtualOffset += total_size;
        memsVirtualAddress = (uintptr_t)memsVirtualOffset;
        appendSegment(mainChain->head, 1, total_size);
        return (void*)(memsVirtualAddress / 4096);
    }

    struct Node* currentNode = mainChain->head;

    while (currentNode) {
        struct Segment* currentSegment = currentNode->segment;

        while (currentSegment) {
            if (currentSegment->type == 0 && currentSegment->size >= total_size) {
                size_t remaining_hole = currentSegment->size - total_size;

                if (remaining_hole > 0) {
                    appendSegment(currentNode, 0, remaining_hole);
                }

                if (currentSegment->prev) {
                    currentSegment->prev->next = currentSegment->next;
                } else {
                    currentNode->segment = currentSegment->next;
                }
                if (currentSegment->next) {
                    currentSegment->next->prev = currentSegment->prev;
                }

                memsVirtualOffset += total_size;
                memsVirtualAddress = (uintptr_t)memsVirtualOffset;
                appendSegment(currentNode, 1, total_size);

                return (void*)(memsVirtualAddress / 4096);
            }

            currentSegment = currentSegment->next;
        }

        if (currentNode->next == NULL) {
            currentNode->next = createNode(total_size);
            currentNode->next->prev = currentNode;

            memsVirtualOffset += total_size;
            memsVirtualAddress = (uintptr_t)memsVirtualOffset;
            appendSegment(currentNode, 1, total_size);

            return (void*)(memsVirtualAddress / 4096);
        }

        currentNode = currentNode->next;
    }
    
    return (void*)(memsVirtualAddress / 4096);
}



/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_stats() {
    int mainChainLength = 0;
    int subChainLength = 0;

    struct Node* currentNode = mainChain->head;

    while (currentNode) {
        mainChainLength++;

        printf("MainChain Node %d -> Size: %d\n", mainChainLength, currentNode->size / 4096);

        struct Segment* currentSegment = currentNode->segment;

        while (currentSegment) {
            subChainLength++;
            printf("  Segment Type: %s, Size: %d\n", currentSegment->type == 1 ? "PROCESS" : "HOLE", currentSegment->size / 4096);

            currentSegment = currentSegment->next;
        }

        currentNode = currentNode->next;
    }

    printf("MainChain Length: %d\n", mainChainLength);
    printf("SubChain Length: %d\n", subChainLength);
}


/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void *mems_get(void*v_ptr){

    void *mems_get(void *v_ptr) {
    uintptr_t memsVirtualAddress = (uintptr_t)v_ptr;
    uintptr_t memsPhysicalAddress = memsVirtualAddress * PAGE_SIZE;
    return (void*)memsPhysicalAddress;
}
    
}


/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/

void mems_free(void *v_ptr) {

    struct Node* currentNode = mainChain->head;
    while (currentNode) {
        struct Segment* currentSegment = currentNode->segment;

        while (currentSegment) {
            if (v_ptr >= (void*)currentSegment && v_ptr < (void*)((char*)currentSegment + currentSegment->size)) {
                currentSegment->type = 0;

                if (currentSegment->prev && currentSegment->prev->type == 0) {
                    currentSegment->prev->size += currentSegment->size;
                    if (currentSegment->next) {
                        currentSegment->next->prev = currentSegment->prev;
                    }
                    currentSegment->prev->next = currentSegment->next;
                    munmap(currentSegment, sizeof(struct Segment));
                    currentSegment = currentSegment->prev;
                }

                if (currentSegment->next && currentSegment->next->type == 0) {
                    currentSegment->size += currentSegment->next->size;
                    if (currentSegment->next->next) {
                        currentSegment->next->next->prev = currentSegment;
                    }
                    currentSegment->next = currentSegment->next->next;
                    munmap(currentSegment->next, sizeof(struct Segment));
                }
                return;
            }
            currentSegment = currentSegment->next;
        }
        currentNode = currentNode->next;
    }
}