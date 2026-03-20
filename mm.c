// mm.c
#include "mm.h"
#include "lib/string.h"

// Базовый адрес для выделения физических страниц
static paddr_t next_page = 0x1000000; // 16MB

// Максимальный размер доступной физической памяти (например, 1static const paddr_t PHYS_MEM_LIMIT = 0x8000000; // 128MB

// Битовая карта для отслеживания использования страниц
// Предположим, у нас есть 128MB / 4KB = 32768 страниц
// 32768 / 8 = 4096 байт для битовой карты
static uint8_t page_bitmap[PHYS_MEM_LIMIT / PAGE_SIZE / 8] = {0};

/**
 * Проверяет, занята ли страница с индексом page_idx
 */
static inline int is_page_used(size_t page_idx) {
    size_t byte_idx = page_idx / 8;
    size_t bit_idx = page_idx % 8;
    return (page_bitmap[byte_idx] & (1 << bit_idx)) != 0;
}

/**
 * Отмечает страницу с индексом page_idx как используемую
 */
static inline void set_page_used(size_t page_idx) {
    size_t byte_idx = page_idx / 8;
    size_t bit_idx = page_idx % 8;
    page_bitmap[byte_idx] |= (1 << bit_idx);
}

/**
 * Отмечает страницу с индексом page_idx как свободную
 */
static inline void set_page_free(size_t page_idx) {
    size_t byte_idx = page_idx / 8;
    size_t bit_idx = page_idx % 8;
    page_bitmap[byte_idx] &= ~(1 << bit_idx);
}

/**
 * Находит первый свободный индекс страницы
 * Возвращает SIZE_MAX, если нет свободных страниц
 */
static size_t find_first_free_page_index() {
    size_t total_pages = PHYS_MEM_LIMIT / PAGE_SIZE;
    for (size_t i = 0; i < total_pages; i++) {
        if (!is_page_used(i)) {
            return i;
        }
    }
    return SIZE_MAX; // Нет свободных страниц
}

paddr_t get_free_page() {
    size_t page_idx = find_first_free_page_index();
    if (page_idx == SIZE_MAX) {
        return 0; // Нет свободных страниц
    }

    paddr_t page_addr = (paddr_t)page_idx * PAGE_SIZE;
    set_page_used(page_idx);
    return page_addr;
}

void free_page(paddr_t page) {
    if (page < 0x1000000 || page >= PHYS_MEM_LIMIT || (page & 0xFFF) != 0) {
        // Неверный адрес страницы
        return;
    }

    size_t page_idx = page / PAGE_SIZE;
    set_page_free(page_idx);
}

/**
 * Получает или создает таблицу страниц (PT) по указателю на PML4/PDPT/PD
 * и индексу. Если PT не существует и create=true, то создается новая.
 */
static uint64_t *get_or_create_table(uint64_t *parent_table, size_t index, int create) {
    uint64_t entry = parent_table[index];
    if (entry & 1) { // Присутствует
        return (uint64_t *)(entry & ~0xFFF);
    } else if (create) {
        paddr_t new_table_phys = get_free_page();
        if (!new_table_phys) return 0;
        uint64_t *new_table_virt = (uint64_t *)new_table_phys;
        memset(new_table_virt, 0, PAGE_SIZE); // Очищаем новую таблицу
        parent_table[index] = new_table_phys | 0x3; // Present + Writable
        return new_table_virt;
    } else {
        return 0; // Не существует и не нужно создавать
    }
}

paddr_t get_physical_address(uint64_t *page_dir, vaddr_t vaddr) {
    if (!page_dir) return 0;

    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF; // bits 39-47
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF; // bits 30-38
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;   // bits 21-29
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;   // bits 12-20

    // Проверяем PML4
    if (!(page_dir[pml4_idx] & 1)) return 0; // PML4 entry not present
    uint64_t *pdpt = (uint64_t *)(page_dir[pml4_idx] & ~0xFFF);
    
    // Проверяем PDPT
    if (!(pdpt[pdpt_idx] & 1)) return 0; // PDPT entry not present
    uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & ~0xFFF);

    // Проверяем PD (проверка на большие страницы)
    uint64_t pd_entry = pd[pd_idx];
    if (pd_entry & 1) {
        if (pd_entry & 0x80) { // Large page (2MB)
            return ((pd_entry & ~((1ULL << 21) - 1)) + (vaddr & ((1ULL << 21) - 1)));
        }
    } else {
        return 0; // PD entry not present
    }
    uint64_t *pt = (uint64_t *)(pd_entry & ~0xFFF);

    // Проверяем PT
    uint64_t pt_entry = pt[pt_idx];
    if (!(pt_entry & 1)) return 0; // PT entry not present

    return ((pt_entry & ~0xFFF) + (vaddr & 0xFFF));
}

void map_page(uint64_t *page_dir, vaddr_t vaddr, paddr_t paddr, int flags) {
    if (!page_dir) return;

    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;

    // Получаем или создаем PDPT
    uint64_t *pdpt = get_or_create_table(page_dir, pml4_idx, 1);
    if (!pdpt) return;

    // Получаем или создаем PD
    uint64_t *pd = get_or_create_table(pdpt, pdpt_idx, 1);
    if (!pd) return;

    // Получаем или создаем PT
    uint64_t *pt = get_or_create_table(pd, pd_idx, 1);
    if (!pt) return;

    // Мапим страницу
    pt[pt_idx] = (paddr & ~0xFFF) | flags;
}

void unmap_page(uint64_t *page_dir, vaddr_t vaddr) {
    if (!page_dir) return;

    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;

    // Проверяем наличие цепочки
    if (!(page_dir[pml4_idx] & 1)) return;
    uint64_t *pdpt = (uint64_t *)(page_dir[pml4_idx] & ~0xFFF);
    
    if (!(pdpt[pdpt_idx] & 1)) return;
    uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & ~0xFFF);

    uint64_t pd_entry = pd[pd_idx];
    if (!(pd_entry & 1) || (pd_entry & 0x80)) return; // Not present or large page
    uint64_t *pt = (uint64_t *)(pd_entry & ~0xFFF);

    // Очищаем запись в PT
    pt[pt_idx] = 0;

    // Инвалидация кэша TLB для конкретного адреса
    __asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}

struct page_table_entry *get_pte(uint64_t *page_dir, vaddr_t vaddr) {
    if (!page_dir) return 0;

    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;

    // Проверяем наличие цепочки
    if (!(page_dir[pml4_idx] & 1)) return 0;
    uint64_t *pdpt = (uint64_t *)(page_dir[pml4_idx] & ~0xFFF);
    
    if (!(pdpt[pdpt_idx] & 1)) return 0;
    uint64_t *pd = (uint64_t *)(pdpt[pdpt_idx] & ~0xFFF);

    uint64_t pd_entry = pd[pd_idx];
    if (!(pd_entry & 1) || (pd_entry & 0x80)) return 0; // Not present or large page
    uint64_t *pt = (uint64_t *)(pd_entry & ~0xFFF);

    return (struct page_table_entry *)&pt[pt_idx];
}

void mm_init() {
    // Инициализируем битовую карту - отмечаем используемую память как занятую
    // Например, первые 16MB могут быть заняты ядром и другими структурами
    size_t used_pages = 0x1000000 / PAGE_SIZE; // 16MB / 4KB = 4096 страниц
    for (size_t i = 0; i < used_pages; i++) {
        set_page_used(i);
    }
    // Также можно зарезервировать другие важные области
    // ...
}

