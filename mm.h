#ifndef MM_H
#define MM_H

#include "types.h"

#define PAGE_SIZE 0x1000

typedef struct {
    uint64_t present : 1;
    uint64_t writable : 1;
    uint64_t user : 1;
    uint64_t writethrough : 1;
    uint64_t cache_disabled : 1;
    uint64_t accessed : 1;
    uint64_t dirty : 1;
    uint64_t size : 1;
    uint64_t global : 1;
    uint64_t available : 3;
    uint64_t nx : 1; // No Execute
    uint64_t reserved : 51;
} __attribute__((packed)) page_table_entry_t;

paddr_t get_free_page();
void free_page(paddr_t page);
paddr_t get_physical_address(uint64_t *page_dir, vaddr_t vaddr);
void map_page(uint64_t *page_dir, vaddr_t vaddr, paddr_t paddr, int flags);
void unmap_page(uint64_t *page_dir, vaddr_t vaddr);
struct page_table_entry *get_pte(uint64_t *page_dir, vaddr_t vaddr);
void mm_init();

#endif
