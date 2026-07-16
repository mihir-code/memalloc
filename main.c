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

const int MAGICAL_BYTES = 0x55;
const int BLOCK_MARKER = 0xDD;
const int PAGE_SIZE = 4096;
const int FIRST_BLOCK_OFFSET = sizeof(block);

static char *heap_start = NULL;

block *find_prev_used_block(block *ptr)
{
    block *mov_ptr = ptr;
    while (mov_ptr->prev != NULL)
    {
        mov_ptr = mov_ptr->prev;
        if (mov_ptr->in_use == true)
        {
            return mov_ptr;
        }
    }
    return NULL;
}

header *get_malloc_header()
{
    assert(heap_start != NULL);
    header *malloc_header = (header *) heap_start;
    assert(malloc_header->magical_bytes == MAGICAL_BYTES);
    return malloc_header;
}
block *find_last_block()
{
    header *malloc_header = get_malloc_header();
    block *mem_block = (block *)(char *)malloc_header + sizeof(header);
    while (mem_block->next != NULL)
    {
        mem_block= mem_block->next;
    }
    return mem_block;
}

void reduce_heap_size()
{
    block *last_block = find_last_block();
    block *prev_used_block = find_prev_used_block(last_block);
    if (prev_used_block == NULL)
    {
        if (last_block->length > PAGE_SIZE)
        {
            last_block->length = PAGE_SIZE;
        }
        prev_used_block = last_block;
    }
    void *new_end = (void *) prev_used_block + sizeof(block) + prev_used_block->length;
    void *heap_end = w_sbrk(0);
    while (new_end < (heap_end - PAGE_SIZE))
    {
        w_sbrk(-PAGE_SIZE);
        heap_end = w_sbrk(0);
        header *mem_header = get_malloc_header();
        mem_header->amount_of_pages -=1;
    }
    if (heap_end - new_end > sizeof(block) + 1) // TODO: review this case
    {
        block *new_not_used = (block *) new_end;
        new_not_used->marker = BLOCK_MARKER;
        new_not_used->in_use = false;
        new_not_used->length = heap_end - new_end - sizeof(block);
        new_not_used->next = NULL;
        new_not_used->prev = prev_used_block;
        prev_used_block->next = new_not_used;
    }
}
int *add_used_block(ssize_t size)
{
    header *malloc_header = get_malloc_header();
    while (malloc_header->lock) // thread safety
    {
        Sleep(1000);
    };
    malloc_header->lock = true;
    block *mem_block = (block *) ((char *) heap_start + sizeof(header));
    block *smallest_block = NULL; // This is needed to store the info in mem_block
    block *last_block = mem_block; // This is to store the prev pointer

    // This is the best fit
    while (mem_block != NULL)
    {
        assert(mem_block->marker == BLOCK_MARKER);
        if ((mem_block->length + sizeof(block)) >= size && mem_block->in_use == false)
        {
            if (smallest_block == NULL || smallest_block->length > mem_block->length)
            {
                smallest_block = mem_block;
            }
        }
        last_block = mem_block;
        mem_block = mem_block->next;
    }
    // I think this is that there isn't enough space so we request more heap space.
    if (smallest_block == NULL)
    {
        block *last_block = find_last_block();
        while (last_block->length < size)
        {
            w_sbrk(4096);
            last_block->length +=4096; // this makes the last free_block 4096 bytes larger
            malloc_header->amount_of_pages +=1;
        }
        smallest_block = last_block;
    }
    smallest_block->in_use = true;
    int end_of_list = smallest_block->length - size - sizeof(block) - 1; // can we add the header space as well
    if (end_of_list <= 0)
    {
        w_sbrk(4096);
        malloc_header->amount_of_pages += 1;
        last_block->length += 4096;
        end_of_list = smallest_block->length - size - sizeof(block) - 1;
    }
    int remaining_size = end_of_list + 1;
    malloc_header->amount_of_blocks += 1;
    block *new_block = (block *) ((char *)smallest_block + sizeof(block) + size);
    new_block->marker = BLOCK_MARKER;
    new_block->prev = smallest_block;
    new_block->next = smallest_block->next;
    if (new_block->next != NULL)
    {
        (new_block->next)->prev = new_block;
    }
    smallest_block->next = new_block;
    new_block->length = remaining_size;
    smallest_block->length = size;
    malloc_header->lock = false;
    return (int *)((char *) smallest_block + sizeof(block));
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

bool an_free(void *ptr)
{
    header *malloc_header = get_malloc_header();
    while(malloc_header->lock)
    {
        Sleep(1000);
    }
    malloc_header->lock = true;
    block *mem_block = ptr - sizeof(block);
    if (mem_block->marker != BLOCK_MARKER){
        return false;
    }
    else{
        mem_block->in_use = false;
        memset(ptr, 0, mem_block->length);
        // forward case
        if (mem_block->next != NULL && (mem_block->next)->in_use == false){ // TODO: There are many edge cases we need to check here
            block *not_used = mem_block->next;
            if (not_used != NULL){
                mem_block->next = not_used->next;
                if (not_used->next != NULL){
                    not_used->next->prev = mem_block;
                }
                else{
                    mem_block->next = NULL;
                }
                mem_block->length += sizeof(block) + not_used->length;
                memset((void *) not_used, 0, mem_block->length); // erase header and data
                malloc_header->amount_of_blocks -=1;
            }
            // backward case
            if (mem_block->prev != NULL && (mem_block->prev)->in_use == false){
                block *del_prev_block = mem_block;
                mem_block = mem_block->prev;
                mem_block->length += sizeof(block) + del_prev_block->length;
                mem_block->next = del_prev_block->next;
                if (mem_block->next != NULL)
                {
                    mem_block->next->prev = mem_block;
                }
                malloc_header->amount_of_blocks -=1;
            }
            reduce_heap_size();
        }
    }
    malloc_header->lock = false;
    return true;


}