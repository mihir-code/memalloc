#include <assert.h>
#include <stdbool.h> // for modern syntax of bool
#include <stddef.h> // ptrdiff_t
#include <stdint.h> // integer will be exactly the exact width it was specified to have
#include <stdio.h>
#include <windows.h>


#define HEAP_RESERVE_SIZE ((size_t)1 << 32) // 2^32 bits reserved address space, macro

static char *heap_base = NULL;   // start of reserved region
static char *heap_break = NULL;  // current break (end of committed, in-use heap)
static char *heap_commit = NULL; // end of committed pages

/// Windows has no sbrk(). We emulate it by reserving one big virtual
/// address range up front and committing more pages into it as the
/// "break" moves forward, same shape as the Unix sbrk contract:
/// w_sbrk(0) returns the current break, w_sbrk(n) grows it by n bytes
/// and returns the break's previous position (or (void*)-1 on failure).
// TODO: fix edge cases like shrinking
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
    uint32_t length;
    bool in_use;
    struct free_area *prev;
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



int *add_used_block(ssize_t size)
{
    header *malloc_header = get_malloc_header();
    while (malloc_header -> lock)
    {
        sleep(1);
    };
    malloc_header -> lock = true;
    block *mem_block = (block *) ((char *) heap_start + sizeof(header));
    block *smallest_block = NULL; // This is needed to store the info in mem_block
    block *last_block = mem_block; // This is to store the prev pointer

    while (mem_block != NULL)
    {   // This is the best fit, if there is space immediately and
        assert(mem_block-> marker == BLOCK_MARKER);
        if ((mem_block-> length + sizeof(block)) >= size && mem_block->in_use == false)
        {
            if (smallest_block == NULL || smallest_block->length > mem_block->length)
            {
                smallest_block = mem_block;
            }
        }
        last_block = mem_block;
        mem_block = mem_block->next;
    }
    if (smallest_block == NULL)
    { // I think this is that there isn't enough space so we request more heap space.
        block *last_block = find_last_block();
        while (last_block-> length < size)
        {
            w_sbrk(4096);
            last_block-> length +=4096;
            malloc_header->amount_of_pages +=1;
        }
        smallest_block = last_block;
    }



}
/// returns an int pointer
int *an_malloc(intptr_t size)
{
    if (heap_start == NULL)
    {
        heap_start = w_sbrk(0);
        w_sbrk(4096);
    }
    char *heap_end = w_sbrk(0); // w_sbrk == set break, 0 returns the current end
    ptrdiff_t length = heap_end - heap_start;
    // Check for magical bytes to be at the beginning of the heap
    if ((*heap_start != MAGICAL_BYTES)){
    *(heap_start) = MAGICAL_BYTES;
    // Setting up the header
    header *malloc_header = (header *) heap_start; // this is a pointer to header
    malloc_header->amount_of_blocks = 1; // dereference the pointer and acccess amount_of_blocks
    malloc_header->amount_of_pages = 1;
    // Setting up the block
    block *first_block = (block*) ((char *) heap_start + sizeof(header));
    first_block -> marker = BLOCK_MARKER;
    first_block -> in_use = false;
    first_block -> length = length - sizeof(header) - sizeof(header);
    first_block -> prev = NULL;
    first_block -> next = NULL;
    }
    return add_used_block(size);

}

