#include <stdbool.h> // for modern syntax of bool
#include <stdint.h> // integer will be exactly the exact width it was specified to have
#include <stdio.h>

int main(void)
{
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

    printf("Hello, World!\n");
    return 0;
}
