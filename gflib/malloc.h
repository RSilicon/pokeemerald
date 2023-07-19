#ifndef GUARD_ALLOC_H
#define GUARD_ALLOC_H

#define HEAP_SIZE 0x1C000

#define FREE_AND_SET_NULL(ptr)          \
{                                       \
    Free(ptr);                          \
    ptr = NULL;                         \
}

#define TRY_FREE_AND_SET_NULL(ptr) if (ptr != NULL) FREE_AND_SET_NULL(ptr)

extern u8 gHeap[0x1C000];

void *Alloc(u32 size);
void *Calloc(u32 size);
void Free(void *pointer);
void ResetHeap(void);

#endif // GUARD_ALLOC_H
