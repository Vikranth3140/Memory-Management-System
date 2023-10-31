/*
All the main functions with respect to the MeMS are inplemented here
read the function discription for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions 
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNSTIONS AND THEIR FUNCTIONALITY
*/
// add other headers as required
#include<stdio.h>
#include<stdlib.h>
#include <sys/mman.h>
#include<unistd.h>

/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this 
macro to make the output of all system same and conduct a fair evaluation. 
*/
#define PAGE_SIZE 4096


/* 
Making the free list structure 
*/

struct MainChainNode {
    int memory_size;
    SubChainNode *sub_chain;
    struct MainChainNode* next;
    struct MainChainNode* prev;
};

struct SubChainNode {
    void* address;
    size_t size;
    int is_mapped;
    struct SubChainNode *prev;
    struct SubChainNode *next;
};

struct MainChainNode* mainchain = NULL;
void* mems_heap_start = NULL;

/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/

void mems_init() {

    mainchain = (struct MainChainNode*)mmap(NULL, sizeof(struct MainChainNode), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mainchain != MAP_FAILED) {
        // Initialize the Main Chain attributes.
        mainchain->memory_size = sizeof(struct MainChainNode); // Set the size dynamically.
        mainchain->sub_chain = NULL; // No sub-chain nodes initially.
        mainchain->next = NULL; // No next node initially.
        mainchain->prev = NULL; // No previous node initially.

        // Set the starting MeMS virtual address.
        mems_heap_start = (void*)0;

    } else {
        printf("Error");
    }

}

/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish(){

    // You should traverse the Main Chain (free list) and unmap any memory segments that were allocated using mmap.
    // This function should release all the memory allocated by your MeMS system.
    MainChainNode* current_mainNode = mainchain;

    while (current_mainNode != NULL) {
        SubChainNode* current_subNode = current_mainNode->sub_chain;

        while (current_subNode != NULL) {
            if (current_subNode->is_mapped) {
                // Unmap the memory segment allocated by mmap.
                munmap(current_subNode->address, current_subNode->size);
            }

            current_subNode = current_subNode->next;
        }

        // Move to the next Main Chain node.
        current_mainNode = current_mainNode->next;
    }

    // Clean up any other resources or data structures used by your MeMS system.
    
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
    
    // Check if there is a sufficiently large segment in the free list.
    MainChainNode* current_mainNode = mainchain;
    while (current_mainNode != NULL) {
        SubChainNode* current_subNode = current_mainNode->sub_chain;
        while (current_subNode != NULL) {
            if (!current_subNode->is_mapped && current_subNode->size >= size) {
                // Reuse this segment.
                current_subNode->is_mapped = 1; // Mark it as mapped.
                return current_subNode->address; // Return the MeMS virtual address.
            }
            current_subNode = current_subNode->next;
        }
        current_mainNode = current_mainNode->next;
    }

    // If no segment is available, use mmap to allocate more memory.
    void* address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (address != MAP_FAILED) {
        // Update the free list with the new segment.
        // Create a new Main Chain node if needed.
        if (mainchain == NULL) {
            mainchain = (MainChainNode*) address;
            mainchain->next = NULL;
            mainchain->prev = NULL;
            mainchain->sub_chain = NULL;
            mainchain->memory_size = size; // Set the total memory size (adjust if needed).
        } else {
            // Traverse the Main Chain to find the last node.
            current_mainNode = mainchain;
            while (current_mainNode->next != NULL) {
                current_mainNode = current_mainNode->next;
            }
            
            // Create a new Main Chain node.
            MainChainNode* new_mainNode = (MainChainNode*) address;
            new_mainNode->next = NULL;
            new_mainNode->prev = current_mainNode;
            new_mainNode->sub_chain = NULL;
            new_mainNode->memory_size = size; // Set the total memory size (adjust if needed).
            
            // Link it to the last node in the Main Chain.
            current_mainNode->next = new_mainNode;
        }

        // Create a new Sub Chain node for the allocated segment.
        SubChainNode* new_subNode = (SubChainNode*)(address + sizeof(MainChainNode));
        new_subNode->address = address + sizeof(MainChainNode);
        new_subNode->size = size;
        new_subNode->is_mapped = 1; // Mark it as mapped.
        new_subNode->prev = NULL;
        new_subNode->next = NULL;

        // Link it to the Main Chain node's sub chain.
        if (current_mainNode->sub_chain == NULL) {
            current_mainNode->sub_chain = new_subNode;
        } else {
            SubChainNode* current_subNode = current_mainNode->sub_chain;
            while (current_subNode->next != NULL) {
                current_subNode = current_subNode->next;
            }
            current_subNode->next = new_subNode;
            new_subNode->prev = current_subNode;
        }

        // Return the MeMS virtual address.
        return new_subNode->address;
    }

    return NULL; // Handle the case where memory allocation using mmap failed.
}

/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_stats(){

    int usedPages = 0;
    int unusedMemory = 0;

    // Iterate through the Main Chain.
    MainChainNode* current_mainNode = mainchain;
    while (current_mainNode != NULL) {
        // Iterate through the Sub Chain.
        SubChainNode* current_subNode = current_mainNode->sub_chain;
        while (current_subNode != NULL) {
            if (current_subNode->is_mapped) {
                // This is a used page (PROCESS).
                usedPages += current_subNode->size / PAGE_SIZE;
            } else {
                // This is an unused memory (HOLE).
                unusedMemory += current_subNode->size;
            }
            current_subNode = current_subNode->next;
        }

        // Print details about the Main Chain node.
        printf("Main Chain Node - Memory Size: %d\n", current_mainNode->memory_size);
        current_mainNode = current_mainNode->next;
    }

    // Print the statistics.
    printf("Used Pages: %d\n", usedPages);
    printf("Unused Memory: %d bytes\n", unusedMemory);

}


/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void *mems_get(void*v_ptr){

    MainChainNode* current_mainNode = mainchain;
    while (current_mainNode != NULL) {
        SubChainNode* current_subNode = current_mainNode->sub_chain;
        while (current_subNode != NULL) {
            if (current_subNode->address == v_ptr) {
                // This virtual address matches. Return the MeMS physical address.
                return v_ptr;  // In this simplified example, the virtual address is treated as the physical address.
                // In a real implementation, you might have a mapping from virtual to physical addresses.
            }
            current_subNode = current_subNode->next;
        }
        current_mainNode = current_mainNode->next;
    }

    return NULL;
}


/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/
void mems_free(void *v_ptr){
    
    MainChainNode* current_mainNode = mainchain;
    while (current_mainNode != NULL) {
        SubChainNode* current_subNode = current_mainNode->sub_chain;
        while (current_subNode != NULL) {
            if (current_subNode->address == v_ptr && current_subNode->is_mapped) {
                // This virtual address matches and is marked as mapped. Mark it as unmapped (HOLE).
                current_subNode->is_mapped = 0;
                return;  // The memory has been freed successfully.
            }
            current_subNode = current_subNode->next;
        }
        current_mainNode = current_mainNode->next;
    }
}