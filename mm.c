#include "mm.h"
#include "lib/string.h"

static paddr_t next_page = 0x1000000; // 16MB

paddr_t get_free_page() {
    paddr_t page = next_page;
    next_page += 0x1000; // PAGE_SIZE
    return page;
}

void free_page(paddr_t page) {
    // Временная заглушка
}

paddr_t get_physical_address(uint64_t *page_dir, vaddr_t vaddr) {
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;
    
    uint64_t *pdpt = (uint64_t *)((page_dir[pdpt_idx]) & ~0xFFF);
    uint64_t *pd = (uint64_t *)((pdpt[pd_idx]) & ~0xFFF);
    uint64_t pt_entry = pd[pt_idx];
    
    if (!(pt_entry & 1)) return 0; // Не присутствует
    
    return ((pt_entry & ~0xFFF) + (vaddr & 0xFFF));
}

void map_page(uint64_t *page_dir, vaddr_t vaddr, paddr_t paddr, int flags) {
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;
    
    uint64_t *pdpt = (uint64_t *)((page_dir[pdpt_idx]) & ~0xFFF);
    uint64_t *pd = (uint64_t *)((pdpt[pd_idx]) & ~0xFFF);
    
    pd[pt_idx] = (paddr & ~0xFFF) | flags;
}

void unmap_page(uint64_t *page_dir, vaddr_t vaddr) {
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;
    
    uint64_t *pdpt = (uint64_t *)((page_dir[pdpt_idx]) & ~0xFFF);
    uint64_t *pd = (uint64_t *)((pdpt[pd_idx]) & ~0xFFF);
    
    pd[pt_idx] = 0; // Очистить запись
}

struct page_table_entry *get_pte(uint64_t *page_dir, vaddr_t vaddr) {
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;
    
    uint64_t *pdpt = (uint64_t *)((page_dir[pdpt_idx]) & ~0xFFF);
    uint64_t *pd = (uint64_t *)((pdpt[pd_idx]) & ~0xFFF);
    return (struct page_table_entry *)&pd[pt_idx];
}

void mm_init() {
    // Инициализация менеджера памяти
}
