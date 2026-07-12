#include <stdbool.h> // for modern syntax of bool
#include <stddef.h> // ptrdiff_t
#include <stdint.h> // integer will be exactly the exact width it was specified to have
#include <stdio.h>
#include <windows.h>

// Windows has no sbrk(). We emulate it by reserving one big virtual
// address range up front and committing more pages into it as the
// "break" moves forward, same shape as the Unix sbrk contract:
// w_sbrk(0) returns the current break, w_sbrk(n) grows it by n bytes
// and returns the break's previous position (or (void*)-1 on failure).
#define HEAP_RESERVE_SIZE ((size_t)1 << 32) // 2^32 bits reserved address space, macro

static char *heap_base = NULL;   // start of reserved region
static char *heap_break = NULL;  // current break (end of committed, in-use heap)
static char *heap_commit = NULL; // end of committed pages

void *w_sbrk(intptr_t increment) // intptr_t is guaranteed to be large enough to hold a pointer to any object
{
    if (heap_base == NULL) // Checks for the first call to the heap
    {
        heap_base = VirtualAlloc(NULL, HEAP_RESERVE_SIZE, MEM_RESERVE, PAGE_READWRITE);
        if (heap_base == NULL) // Checks for if  VirualAlloc returns null on
        {
            return (void *) -1; // shows that there is some error
        }
        heap_break = heap_base; // if VirtualAlloc call goes through
        heap_commit = heap_base;
    }

    char *old_break = heap_break;
    char *new_break = heap_break + increment; // it can shrink the heap as well

    if (new_break > heap_commit)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        size_t page_size = si.dwPageSize;
        size_t needed = new_break - heap_commit;
        size_t commit_size = ((needed + page_size - 1) / page_size) * page_size; // we need to commit a multiple of the page size

        if (VirtualAlloc(heap_commit, commit_size, MEM_COMMIT, PAGE_READWRITE) == NULL)
        {
            return (void *) -1;
        }
        heap_commit += commit_size;
    }

    heap_break = new_break;
    return old_break;
}

int main(void)
{

    return 10;
}


const int MAGICAL_BYTES = 0x55;
const int BLOCK_MARKER = 0xDD;
struct free_area // block of memory
{
    uint8_t marker;
    struct free_area *prev;
    bool in_use;
    uint32_t length;
    struct free_area *next;
};
struct stats
{
    int magical_bytes;
    bool lock;
    uint32_t amount_of_blocks;
    uint64_t amount_of_pages;
};
typedef struct stats header;
typedef struct free_area block;

static char *heap_start = NULL;

int *an_malloc(intptr_t size)
{
    if (heap_start == NULL)
    {
        heap_start = w_sbrk(0);
        w_sbrk(4096);
    }
    char *heap_end = w_sbrk(0); // w_sbrk == set break, 0 returns the current end
    ptrdiff_t length = heap_end - heap_start;
    if ((uint8_t) (*heap_start) != MAGICAL_BYTES)
    {

    }
    return NULL;
}
