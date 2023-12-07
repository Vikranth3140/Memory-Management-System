# Custom Memory Management System
[Documentation](https://docs.google.com/document/d/1Gs9kC3187lLrinvK1SueTc8dHCJ0QP43eRlrCRlXiCY/edit?usp=sharing)
---

### How to run the example.c
After implementing functions in mems.h follow the below steps to run example.c file
```
$ make
$ ./example
```

## Introduction

MeMS is a custom memory management system that allows for efficient allocation and deallocation of memory using a doubly linked list data structure. It also provides the capability to map virtual memory to physical memory.

## Main Data Structures

### 1. MainChain

- struct MainChain is the top-level data structure that keeps track of the memory management process.
- It contains a pointer to the head node of the linked list.

### 2. Node

- struct Node represents a block of memory in the MeMS system.
- It contains a size field indicating the total size of the memory block, a pointer to the previous node, a pointer to the next node, and a pointer to the first segment in the sub-chain.

### 3. Segment

- struct Segment represents a part of the memory block in a Node. It can be a "PROCESS" segment or a "HOLE" segment.
- It contains a type field (1 for "PROCESS" and 0 for "HOLE"), a size field indicating the size of the segment, a pointer to the previous segment, a pointer to the next segment, and a pointer to the main node it belongs to.

## Initialization

### mems_init()

- This function initializes the MeMS system.
- It creates an empty main chain by setting the head of the chain to NULL.

## Memory Allocation

### mems_malloc(size_t size)

- Allocates memory of the specified size.
- It checks the main chain for available memory segments.
- If a large enough segment is found, it's allocated for the process and returned.
- If no suitable segment is available, a new node and segment are created for the requested memory size.
- It also updates the virtual address and returns the allocated memory's virtual address (divided by 4096).

## Memory Deallocation

### mems_free(void *v_ptr)

- Frees the memory pointed to by the virtual address v_ptr and adds it to the free list.
- It marks the segment as a "HOLE."
- It attempts to merge adjacent hole segments, optimizing memory usage.

## Physical to Virtual Address Mapping

### mems_get(void *v_ptr)

- Maps a physical address to a virtual address.
- Given a virtual address, it calculates the corresponding physical address using the MeMS system's page size (PAGE_SIZE).

## Printing System Stats

### mems_print_stats()

- Prints system statistics, including:
  - The number of pages utilized by mems_malloc.
  - The amount of unused memory (memory in the free list that is not used).
  - Details about each node in the main chain.
  - Details about each segment (PROCESS or HOLE) in the sub-chain.

## Memory Management Workflow

1. *Initialization*: Use mems_init() to set up the MeMS system.

2. *Memory Allocation*: Use mems_malloc(size_t size) to allocate memory.

3. *Physical to Virtual Address Mapping*: Use mems_get(void *v_ptr) to map physical addresses to virtual addresses if needed.

4. *Memory Deallocation*: Use mems_free(void *v_ptr) to free memory and potentially merge adjacent holes.

5. *Finalization*: Call mems_finish() to deallocate all memory and clean up.

## Page Size

- The PAGE_SIZE macro is used throughout the MeMS system to ensure consistent behavior on different systems. It can be modified to match the system's page size, allowing fair evaluation across different environments.
