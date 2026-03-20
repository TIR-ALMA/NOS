#include "sched.h"
#include "mm.h"
#include "lib/string.h"

struct process processes[MAX_PROCESSES];
struct process *current_process = NULL;
struct process *process_queue = NULL;

struct process *create_process() {
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if(processes[i].state == PROC_FREE) {
            // Обнулить всю структуру процесса
            memset(&processes[i], 0, sizeof(struct process));
            
            processes[i].state = PROC_READY;
            processes[i].id = i;
            processes[i].timeslice = 10;
            processes[i].ticks_remaining = 10;
            
            // Выделить директорию страниц
            paddr_t page_dir_phys = get_free_page();
            if (!page_dir_phys) {
                return NULL;
            }
            processes[i].page_dir = (uint64_t*)page_dir_phys;
            // Обнулить директорию страниц
            memset(processes[i].page_dir, 0, PAGE_SIZE);
            
            processes[i].heap_start = 0x200000;
            processes[i].heap_brk = 0x200000;
            processes[i].heap_end = 0x400000;
            processes[i].parent_id = 0;
            processes[i].umask = 022;
            processes[i].blocked_signals = 0;
            
            // Инициализировать регистры
            paddr_t regs_phys = get_free_page();
            if (!regs_phys) {
                free_page((paddr_t)processes[i].page_dir);
                return NULL;
            }
            processes[i].regs = (struct registers*)regs_phys;
            memset(processes[i].regs, 0, sizeof(struct registers));
            
            processes[i].regs->rip = 0x400000; // Точка входа процесса
            processes[i].regs->rsp = 0x500000; // Стек процесса
            processes[i].regs->rflags = 0x202; // Interrupt flag enabled
            processes[i].regs->cr3 = (uint64_t)processes[i].page_dir; // Адрес директории страниц
            
            // Инициализировать очередь процессов
            processes[i].next = &processes[i];
            processes[i].prev = &processes[i];
            
            return &processes[i];
        }
    }
    return NULL;
}

void switch_to_process(struct registers *regs) {
    if (!current_process || !regs) return;
    
    // Сохраняем текущий контекст
    __asm__ volatile (
        "mov %%rax, %0\n\t"
        "mov %%rbx, %1\n\t"
        "mov %%rcx, %2\n\t"
        "mov %%rdx, %3\n\t"
        "mov %%rsi, %4\n\t"
        "mov %%rdi, %5\n\t"
        "mov %%rbp, %6\n\t"
        "mov %%r8, %7\n\t"
        "mov %%r9, %8\n\t"
        "mov %%r10, %9\n\t"
        "mov %%r11, %10\n\t"
        "mov %%r12, %11\n\t"
        "mov %%r13, %12\n\t"
        "mov %%r14, %13\n\t"
        "mov %%r15, %14\n\t"
        "mov %%rsp, %15\n\t"      // Сохраняем RSP последним
        : "=m"(current_process->regs->rax),
          "=m"(current_process->regs->rbx),
          "=m"(current_process->regs->rcx),
          "=m"(current_process->regs->rdx),
          "=m"(current_process->regs->rsi),
          "=m"(current_process->regs->rdi),
          "=m"(current_process->regs->rbp),
          "=m"(current_process->regs->r8),
          "=m"(current_process->regs->r9),
          "=m"(current_process->regs->r10),
          "=m"(current_process->regs->r11),
          "=m"(current_process->regs->r12),
          "=m"(current_process->regs->r13),
          "=m"(current_process->regs->r14),
          "=m"(current_process->regs->r15),
          "=m"(current_process->regs->rsp)  // Сохраняем текущий RSP
        :
        : "memory"
    );
    
    // Загружаем новый контекст
    __asm__ volatile (
        "mov %0, %%rsp\n\t"       // Загружаем новый RSP первым
        "mov %1, %%rax\n\t"
        "mov %2, %%rbx\n\t"
        "mov %3, %%rcx\n\t"
        "mov %4, %%rdx\n\t"
        "mov %5, %%rsi\n\t"
        "mov %6, %%rdi\n\t"
        "mov %7, %%rbp\n\t"
        "mov %8, %%r8\n\t"
        "mov %9, %%r9\n\t"
        "mov %10, %%r10\n\t"
        "mov %11, %%r11\n\t"
        "mov %12, %%r12\n\t"
        "mov %13, %%r13\n\t"
        "mov %14, %%r14\n\t"
        "mov %15, %%r15\n\t"
        "push %16\n\t"            // Загружаем флаги
        "popfq\n\t"
        :
        : "m"(regs->rsp),         // 0
          "m"(regs->rax),         // 1
          "m"(regs->rbx),         // 2
          "m"(regs->rcx),         // 3
          "m"(regs->rdx),         // 4
          "m"(regs->rsi),         // 5
          "m"(regs->rdi),         // 6
          "m"(regs->rbp),         // 7
          "m"(regs->r8),          // 8
          "m"(regs->r9),          // 9
          "m"(regs->r10),         // 10
          "m"(regs->r11),         // 11
          "m"(regs->r12),         // 12
          "m"(regs->r13),         // 13
          "m"(regs->r14),         // 14
          "m"(regs->r15),         // 15
          "m"(regs->rflags)       // 16
        : "memory"
    );
    
    // Загружаем CR3 (адрес директории страниц)
    __asm__ volatile (
        "mov %0, %%cr3\n\t"
        :
        : "r"(regs->cr3)
        : "memory"
    );
}

struct process *copy_process(struct process *parent) {
    if (!parent) return NULL;
    
    struct process *child = create_process();
    if(!child) return NULL;
    
    // Копировать данные процесса
    child->state = PROC_READY;
    child->id = child - processes; // Убедиться, что ID соответствует индексу
    child->parent_id = parent->id;
    child->timeslice = parent->timeslice;
    child->ticks_remaining = parent->ticks_remaining;
    child->heap_start = parent->heap_start;
    child->heap_brk = parent->heap_brk;
    child->heap_end = parent->heap_end;
    child->umask = parent->umask;
    child->blocked_signals = parent->blocked_signals;
    child->exit_code = parent->exit_code;
    
    // Копировать имя
    strncpy(child->name, parent->name, sizeof(child->name) - 1);
    child->name[sizeof(child->name) - 1] = '\0';
    
    // Копировать директорию страниц (реализация COW или полное копирование)
    // В простом варианте - используем общую директорию страниц
    child->page_dir = parent->page_dir;
    
    // Копировать регистры
    memcpy(child->regs, parent->regs, sizeof(struct registers));
    
    // Копировать открытые файлы
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        child->open_files[i] = parent->open_files[i];
    }
    
    // Копировать таблицу сигналов
    for(int i = 0; i < 64; i++) {
        child->signal_handlers[i] = parent->signal_handlers[i];
    }
    
    // Копировать стек сигнала
    child->signal_stack = parent->signal_stack;
    child->signal_stack_size = parent->signal_stack_size;
    child->signal_stack_active = parent->signal_stack_active;
    
    return child;
}

void sched_init() {
    // Инициализация планировщика
    // Все процессы уже обнулены в глобальном массиве
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROC_FREE;
        processes[i].id = -1;
    }
    
    current_process = NULL;
    process_queue = NULL;
}

void schedule() {
    if (!process_queue) return;

    struct process *next = current_process ? current_process->next : process_queue;
    struct process *start = current_process ? current_process : process_queue;
    
    // Найти следующий готовый процесс
    int attempts = 0;
    while (next->state != PROC_READY && attempts < MAX_PROCESSES) {
        next = next->next;
        if (next == start) break;
        attempts++;
    }
    
    if (next->state == PROC_READY) {
        if (current_process) {
            current_process->state = PROC_READY;
        }
        
        current_process = next;
        current_process->state = PROC_RUNNING;
        current_process->ticks_remaining = current_process->timeslice;
        
        switch_to_process(current_process->regs);
    } else if (current_process && current_process->state == PROC_RUNNING) {
        // Если нет готовых процессов, остаться с текущим
        current_process->ticks_remaining = current_process->timeslice;
        // Контекст уже загружен, можно просто вернуться
    }
    // Если нет ни одного готового процесса (включая текущий), система зависнет
}

