/*
All the main functions with respect to the MeMS are inplemented here
read the function discription for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNCTIONS AND THEIR FUNCTIONALITY
*/

// add other headers as required
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this
macro to make the output of all system same and conduct a fair evaluation.
*/
#define PAGE_SIZE 4096
#define MAP_ANONYMOUS 0x20

struct chainNode *head;
size_t virtualAddressStart;

void *allocate_memory_mmap(size_t size)
{
    void *alloted_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (alloted_memory == MAP_FAILED)
    {
        perror("Error while allocating memory using mmap\n");
        exit(EXIT_FAILURE);
    }
    return alloted_memory;
}

void *deallocate_memory_munmap(void *alloted_memory, size_t size)
{
    if (munmap(alloted_memory, size) == -1)
    {
        perror("Error while de-allocating memory using munmap\n");
        exit(EXIT_FAILURE);
    }
}

typedef struct subChainNode
{
    struct subChainNode *next;
    struct subChainNode *prev;
    int type;

    size_t v_ptr_start_index;
    size_t chunk_size;
} subChainNode;

// constructor
subChainNode *createSubChainNode(int type, size_t size, size_t v_ptr_start_index)
{
    subChainNode *newNode = (subChainNode *)allocate_memory_mmap(sizeof(subChainNode));
    newNode->next = NULL;
    newNode->prev = NULL;
    newNode->type = type;

    newNode->chunk_size = size;
    newNode->v_ptr_start_index = v_ptr_start_index;
    return newNode;
}

typedef struct chainNode
{
    struct chainNode *next;
    struct chainNode *prev;
    struct subChainNode *subChainHead;

    size_t seg_size;
    size_t v_ptr_start;
    void *p_ptr;
    void *v_ptr;

    void *store_v_to_free;
} chainNode;

chainNode *createChainNode(size_t seg_size)
{
    chainNode *newNode = (chainNode *)allocate_memory_mmap(sizeof(chainNode));
    newNode->next = NULL;
    newNode->prev = NULL;
    newNode->subChainHead = NULL;

    newNode->seg_size = seg_size;
    newNode->v_ptr_start = virtualAddressStart;
    virtualAddressStart = virtualAddressStart + seg_size;

    newNode->p_ptr = allocate_memory_mmap(seg_size);

    newNode->store_v_to_free = allocate_memory_mmap(sizeof(size_t) * seg_size);
    size_t *ptr = (size_t *)newNode->store_v_to_free;
    for (size_t i = 0; i < seg_size; i++)
    {
        ptr[i] = (size_t)newNode->v_ptr_start + i;
    }
    newNode->v_ptr = (void *)(*ptr);
    return newNode;
}

/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_init()
{
    head = NULL;
    virtualAddressStart = 1000;
}

/*
This function will be called at the end of the MeMS system and its main job is to unmap the
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish()
{
    for (chainNode *temp = head; temp != NULL;)
    {
        for (subChainNode *subTemp = temp->subChainHead; subTemp != NULL;)
        {
            subChainNode *to_delete = subTemp;
            subTemp = subTemp->next;

            to_delete->next = NULL;
            to_delete->prev = NULL;
            deallocate_memory_munmap(to_delete, sizeof(subChainNode));
        }

        chainNode *to_delete_main = temp;
        temp = temp->next;

        deallocate_memory_munmap(to_delete_main->p_ptr, to_delete_main->seg_size);
        deallocate_memory_munmap(to_delete_main->store_v_to_free, to_delete_main->seg_size);
        deallocate_memory_munmap(to_delete_main, sizeof(chainNode));
    }
    mems_init();
}

void split_subChainNode(subChainNode *node, size_t size, size_t index)
{
    if (node->chunk_size == size)
    {
        return;
    }

    subChainNode *newNode = createSubChainNode(1, node->chunk_size - size, index);

    newNode->next = node->next;
    if (node->next != NULL)
    {
        node->next->prev = newNode;
    }

    node->next = newNode;
    newNode->prev = node;
    node->chunk_size = size;
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
void *mems_malloc(size_t size)
{
    size_t newNodesize = ((size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    if (head == NULL)
    {
        head = createChainNode(newNodesize);
        head->subChainHead = createSubChainNode(0, newNodesize, 0);
        split_subChainNode(head->subChainHead, size, size);
        return head->v_ptr;
    }

    chainNode *temp = head;
    chainNode *last = NULL;
    while (temp != NULL)
    {
        size_t index = 0;
        subChainNode *subTemp = temp->subChainHead;
        while (subTemp != NULL)
        {
            if (subTemp->type == 1 && subTemp->chunk_size >= size)
            {
                index = index + size;
                split_subChainNode(subTemp, size, index);

                subTemp->type = 0;
                return temp->v_ptr + subTemp->v_ptr_start_index;
            }
            index = index + subTemp->chunk_size;
            subTemp = subTemp->next;
        }
        last = temp;
        temp = temp->next;
    }

    chainNode *newNode = createChainNode(newNodesize);
    newNode->subChainHead = createSubChainNode(0, newNodesize, 0);
    split_subChainNode(newNode->subChainHead, size, size);

    last->next = newNode;
    newNode->prev = last;
    return newNode->v_ptr;
}

/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_stats()
{
    printf("----- MeMS SYSTEM STATS -----\n");
    size_t main_chain_length = 0;
    for (chainNode *temp = head; temp != NULL; temp = temp->next)
    {
        main_chain_length++;
    }

    size_t pages_used = 0;
    size_t space_unused = 0;
    size_t *subchain_length_array = (size_t *)allocate_memory_mmap(sizeof(size_t) * main_chain_length);
    size_t subchain_num = 0;
    for (chainNode *temp = head; temp != NULL; temp = temp->next)
    {
        printf("MAIN[%lu:%lu]-> ", temp->v_ptr_start, temp->v_ptr_start + temp->seg_size - 1);
        size_t subchain_length = 0;

        int d = temp->v_ptr_start;
        for (subChainNode *subTemp = temp->subChainHead; subTemp != NULL; subTemp = subTemp->next)
        {
            if (subTemp->type == 0)
            {
                printf("P[%lu:%lu] <-> ", d + subTemp->v_ptr_start_index, d + subTemp->v_ptr_start_index + subTemp->chunk_size - 1);
            }
            else
            {
                printf("H[%lu:%lu] <-> ", d + subTemp->v_ptr_start_index, d + subTemp->v_ptr_start_index + subTemp->chunk_size - 1);
                space_unused += subTemp->chunk_size;
            }
            subchain_length++;
        }
        printf(" NULL\n");

        pages_used = pages_used + temp->seg_size / PAGE_SIZE;
        subchain_length_array[subchain_num] = subchain_length;
        subchain_num++;
    }

    printf("Pages used: %lu\n", pages_used);
    printf("Space unused: %lu\n", space_unused);
    printf("Main Chain Length: %lu\n", main_chain_length);
    printf("Sub-chain Length array: [");
    for (int i = 0; i < main_chain_length; i++)
    {
        printf("%lu, ", subchain_length_array[i]);
    }
    printf("]\n");
    printf("-----------------------------\n");

    deallocate_memory_munmap(subchain_length_array, sizeof(size_t) * main_chain_length);
}

/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void *mems_get(void *v_ptr)
{
    for (chainNode *temp = head; temp != NULL; temp = temp->next)
    {
        if (temp->v_ptr <= v_ptr && v_ptr < temp->v_ptr + temp->seg_size)
        {
            return temp->p_ptr + (size_t)v_ptr - (size_t)temp->v_ptr;
        }
    }
}

void fragment_memory(subChainNode *head)
{
    subChainNode *temp = head;
    while (temp != NULL)
    {
        if (temp->type == 1)
        {
            if (temp->prev != NULL && temp->prev->type == 1)
            {
                temp->prev->chunk_size = temp->prev->chunk_size + temp->chunk_size;
                temp->prev->next = temp->next;
                if (temp->next != NULL)
                {
                    temp->next->prev = temp->prev;
                }
                subChainNode *to_delete = temp;
                temp = temp->prev;

                to_delete->next = NULL;
                to_delete->prev = NULL;
                deallocate_memory_munmap(to_delete, sizeof(subChainNode));
            }
        }
        temp = temp->next;
    }
}

/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: nothing
*/
void mems_free(void *v_ptr)
{
    for (chainNode *temp = head; temp != NULL; temp = temp->next)
    {
        if (temp->v_ptr <= v_ptr && v_ptr < temp->v_ptr + temp->seg_size)
        {
            size_t index = (size_t)v_ptr - (size_t)temp->v_ptr;
            subChainNode *subTemp = temp->subChainHead;
            while (subTemp != NULL)
            {
                if (subTemp->v_ptr_start_index == index)
                {
                    subTemp->type = 1;
                    fragment_memory(temp->subChainHead);
                    break;
                }
                subTemp = subTemp->next;
            }
            break;
        }
    }
}