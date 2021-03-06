#include <stdio.h>
#include <stdint.h>

#include "include/hardware.h"
#include "swap.h"

static FILE *swap_file;

char init_swap(const char *path) 
{
    swap_file = fopen(".swap_file", "w+"); /* w+: create, read, write*/
    return swap_file == NULL ? -1 : 0;
}

char store_to_swap(int vpage, int ppage) 
{
    if (fseek(swap_file, vpage << 12, SEEK_SET) == -1) 
        return -1;
    if (fwrite((void*)((ppage << 12) | (uintptr_t)physical_memory), 1, PAGE_SIZE, swap_file) == -1) 
        return -1;
  return 0;
}

char fetch_from_swap(int vpage, int ppage) 
{
    if (fseek(swap_file, vpage << 12, SEEK_SET) == -1) 
        return -1;
    if (fread((void*)((ppage << 12) | (uintptr_t)physical_memory), 
	      1, PAGE_SIZE, swap_file) == -1)       
        return -1;
    return 0;
}
