#include "types.h"
#include "mm.h"
#include "sched.h"
#include "vfs.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "drivers/disk.h"
#include "drivers/network/i8254x.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "liquid_nn.h"
#include "simple_terminal.h"

struct process processes[MAX_PROCESSES];
struct process *current_process = NULL;
struct process *process_queue = NULL;

void find_and_init_network_card(void);

void kernel_main() {
    vga_init();
    printf("Kernel initialized\n");

    mm_init();
    sched_init();
    vfs_init();
    timer_init();
    keyboard_init();
    
    // Инициализация сетевой карты
    find_and_init_network_card();
    
    // Инициализация Liquid Neural Network
    liquid_init();

    // Create initial process
    struct process *init_proc = create_process();
    if(init_proc) {
        strcpy(init_proc->name, "init");
        current_process = init_proc;
        current_process->state = PROC_RUNNING;
        
        process_queue = init_proc;
        init_proc->next = init_proc;
        init_proc->prev = init_proc;
    }

    // Enable interrupts
    __asm__ volatile("sti");

    simple_terminal_run();
}

void schedule() {
    if(!process_queue) return;

    struct process *next = current_process->next;
    struct process *start = current_process;
    do {
        if(next->state == PROC_READY) {
            current_process = next;
            current_process->state = PROC_RUNNING;
            current_process->ticks_remaining = current_process->timeslice;
            switch_to_process(current_process->regs);
            return;
        }
        next = next->next;
    } while(next != start);
    
    // If no ready process found, stay with current
    current_process->state = PROC_RUNNING;
    current_process->ticks_remaining = current_process->timeslice;
    switch_to_process(current_process->regs);
}

void page_fault_handler(uint64_t error_code, vaddr_t fault_addr) {
    if(error_code & 0x1) {
        // Protection violation
        paddr_t old_page = get_physical_address(current_process->page_dir, fault_addr & ~(PAGE_SIZE-1));
        if(old_page) {
            // Copy-on-write or other handling
            printf("Page protection fault at %x, error code: %x\n", fault_addr, error_code);
            while(1);
        } else {
            // Allocate new page for non-present page
            paddr_t new_page = get_free_page();
            if(new_page) {
                map_page(current_process->page_dir, fault_addr & ~(PAGE_SIZE-1), new_page, 7);
            } else {
                printf("Out of memory for page fault at %x\n", fault_addr);
                while(1);
            }
        }
    } else if(error_code & 0x2) {
        // Write to read-only page - implement COW if needed
        paddr_t old_page = get_physical_address(current_process->page_dir, fault_addr & ~(PAGE_SIZE-1));
        if(old_page) {
            paddr_t new_page = get_free_page();
            if(new_page) {
                memcpy((void*)new_page, (void*)old_page, PAGE_SIZE);
                map_page(current_process->page_dir, fault_addr & ~(PAGE_SIZE-1), new_page, 7);
            } else {
                printf("Out of memory for COW at %x\n", fault_addr);
                while(1);
            }
        } else {
            printf("Invalid COW access at %x\n", fault_addr);
            while(1);
        }
    } else {
        printf("Page fault at %x, error code: %x\n", fault_addr, error_code);
        while(1);
    }
}

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct gdt_entry gdt[5];

void init_idt() {
    for(int i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0x08; // Code segment
        idt[i].zero = 0;
        idt[i].type_attr = 0x8E; // Present, DPL=0, Type=E
        idt[i].offset_high = 0;
    }
    
    // Set up specific handlers
    set_idt_entry(0, (uint64_t)isr0);
    set_idt_entry(1, (uint64_t)isr1);
    set_idt_entry(2, (uint64_t)isr2);
    set_idt_entry(3, (uint64_t)isr3);
    set_idt_entry(4, (uint64_t)isr4);
    set_idt_entry(5, (uint64_t)isr5);
    set_idt_entry(6, (uint64_t)isr6);
    set_idt_entry(7, (uint64_t)isr7);
    set_idt_entry(8, (uint64_t)isr8);
    set_idt_entry(9, (uint64_t)isr9);
    set_idt_entry(10, (uint64_t)isr10);
    set_idt_entry(11, (uint64_t)isr11);
    set_idt_entry(12, (uint64_t)isr12);
    set_idt_entry(13, (uint64_t)isr13);
    set_idt_entry(14, (uint64_t)page_fault_stub); // Page fault
    set_idt_entry(15, (uint64_t)isr15);
    set_idt_entry(16, (uint64_t)isr16);
    set_idt_entry(17, (uint64_t)isr17);
    set_idt_entry(18, (uint64_t)isr18);
    set_idt_entry(19, (uint64_t)isr19);
    set_idt_entry(20, (uint64_t)isr20);
    set_idt_entry(21, (uint64_t)isr21);
    set_idt_entry(22, (uint64_t)isr22);
    set_idt_entry(23, (uint64_t)isr23);
    set_idt_entry(24, (uint64_t)isr24);
    set_idt_entry(25, (uint64_t)isr25);
    set_idt_entry(26, (uint64_t)isr26);
    set_idt_entry(27, (uint64_t)isr27);
    set_idt_entry(28, (uint64_t)isr28);
    set_idt_entry(29, (uint64_t)isr29);
    set_idt_entry(30, (uint64_t)isr30);
    set_idt_entry(31, (uint64_t)isr31);
    
    // Set up IRQ handlers
    set_idt_entry(32, (uint64_t)timer_stub);
    set_idt_entry(33, (uint64_t)keyboard_stub);
}

void set_idt_entry(int index, uint64_t handler) {
    idt[index].offset_low = handler & 0xFFFF;
    idt[index].offset_high = (handler >> 16) & 0xFFFF;
    idt[index].selector = 0x08;
    idt[index].zero = 0;
    idt[index].type_attr = 0x8E;
}

void init_gdt() {
    gdt[0] = (struct gdt_entry){0}; // Null descriptor
    gdt[1] = (struct gdt_entry){
        .limit_low = 0xFFFF,
        .base_low = 0x0000,
        .base_middle = 0x00,
        .access = 0x9A, // Present, Ring 0, Code
        .granularity = 0xAF, // 4KB, 32-bit
        .base_high = 0x00
    };
    gdt[2] = (struct gdt_entry){
        .limit_low = 0xFFFF,
        .base_low = 0x0000,
        .base_middle = 0x00,
        .access = 0x92, // Present, Ring 0, Data
        .granularity = 0xCF, // 4KB, 32-bit
        .base_high = 0x00
    };
    gdt[3] = (struct gdt_entry){
        .limit_low = 0xFFFF,
        .base_low = 0x0000,
        .base_middle = 0x00,
        .access = 0xFA, // Present, Ring 3, Code
        .granularity = 0xAF, // 4KB, 32-bit
        .base_high = 0x00
    };
    gdt[4] = (struct gdt_entry){
        .limit_low = 0xFFFF,
        .base_low = 0x0000,
        .base_middle = 0x00,
        .access = 0xF2, // Present, Ring 3, Data
        .granularity = 0xCF, // 4KB, 32-bit
        .base_high = 0x00
    };
}

void load_idt() {
    struct idt_ptr idtr;
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

void load_gdt() {
    struct gdt_ptr gdtr;
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;
    __asm__ volatile ("lgdt %0" : : "m"(gdtr));
}

void timer_stub();
void keyboard_stub();
void page_fault_stub();
void isr0();
void isr1();
void isr2();
void isr3();
void isr4();
void isr5();
void isr6();
void isr7();
void isr8();
void isr9();
void isr10();
void isr11();
void isr12();
void isr13();
void isr14();
void isr15();
void isr16();
void isr17();
void isr18();
void isr19();
20();
void isr21();
void isr22();
void isr23();
void isr24();
void isr25();
void isr26();
void isr27();
void isr28();
void isr29();
void isr30();
void isr31();

void find_and_init_network_card() {
    // Search through PCI devices for Intel 8254x NIC
    // Vendor ID for Intel: 0x8086
    for(uint8_t bus = 0; bus < 256; bus++) {
        for(uint8_t device = 0; device < 32; device++) {
            uint32_t vendor_device = pci_read_reg(bus, device, 0, 0x00);
            uint16_t vendor_id = vendor_device & 0xFFFF;
            uint16_t device_id = vendor_device >> 16;
            
            // Check if it's an Intel 8254x device
            if(vendor_id == 0x8086 && 
               (device_id == 0x100E || device_id == 0x100F || 
                device_id == 0x1013 || device_id == 0x1014 ||
                device_id == 0x1015 || device_id == 0x1016 ||
                device_id == 0x1017 || device_id == 0x1018 ||
                device_id == 0x1019 || device_id == 0x101A ||
                device_id == 0x101D || device_id == 0x101E ||
                device_id == 0x1026 || device_id == 0x1027 ||
                device_id == 0x1028 || device_id == 0x1079 ||
                device_id == 0x107A || device_id == 0x107B ||
                device_id == 0x107C || device_id == 0x107D ||
                device_id == 0x107E || device_id == 0x107F ||
                device_id == 0x1096 || device_id == 0x1098 ||
                device_id == 0x1099 || device_id == 0x1010 ||
                device_id == 0x1012 || device_id == 0x1011 ||
                device_id == 0x101E || device_id == 0x101F)) {
                
                printf("Found Intel 8254x NIC at %d:%d\n", bus, device);
                i8254x_init(bus, device);
                return;
            }
        }
    }
    printf("No Intel 8254x NIC found\n");
}

void timer_interrupt_handler() {
    if(current_process) {
        current_process->ticks_remaining--;
        if(current_process->ticks_remaining <= 0) {
            current_process->state = PROC_READY;
            schedule();
        }
    }
}

void keyboard_interrupt_handler() {
    keyboard_handler();
}

void interrupt_handler(int int_no, struct registers *regs) {
    switch(int_no) {
        case 0x20: // Timer interrupt
            timer_interrupt_handler();
            break;
        case 0x21: // Keyboard interrupt
            keyboard_interrupt_handler();
            break;
        case 0x23: // IRQ11 - предполагаем, что сетевая карта использует IRQ 11
            i8254x_interrupt_handler();
            break;
        case 0xE: // Page fault
            extern void page_fault_handler(uint64_t error_code, vaddr_t fault_addr);
            page_fault_handler(regs->error_code, regs->fault_addr);
            break;
        case 0x80: // System call
            handle_syscall(regs);
            break;
        default:
            printf("Unhandled interrupt: %d\n", int_no);
            break;
    }
}

void handle_syscall(struct registers *regs) {
    switch(regs->rax) {
        case 0: // sys_exit
            regs->rax = sys_exit(regs->rdi);
            break;
        case 1: // sys_read
            regs->rax = sys_read(regs->rdi, (void*)regs->rsi, regs->rdx);
            break;
        case 2: // sys_write
            regs->rax = sys_write(regs->rdi, (void*)regs->rsi, regs->rdx);
            break;
        case 3: // sys_open
            regs->rax = sys_open((char*)regs->rdi, regs->rsi, regs->rdx);
            break;
        case 4: // sys_close
            regs->rax = sys_close(regs->rdi);
            break;
        case 5: // sys_stat
            regs->rax = sys_stat((char*)regs->rdi, (struct stat*)regs->rsi);
            break;
        case 6: // sys_fstat
            regs->rax = sys_fstat(regs->rdi, (struct stat*)regs->rsi);
            break;
        case 7: // sys_lstat
            regs->rax = sys_lstat((char*)regs->rdi, (struct stat*)regs->rsi);
            break;
        case 8: // sys_poll
            regs->rax = sys_poll((struct pollfd*)regs->rdi, regs->rsi, regs->rdx);
            break;
        case 9: // sys_lseek
            regs->rax = sys_lseek(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 10: // sys_mmap
            regs->rax = sys_mmap((void*)regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
            break;
        case 11: // sys_mprotect
            regs->rax = sys_mprotect((void*)regs->rdi, regs->rsi, regs->rdx);
            break;
        case 12: // sys_munmap
            regs->rax = sys_munmap((void*)regs->rdi, regs->rsi);
            break;
        case 13: // sys_brk
            regs->rax = sys_brk((void*)regs->rdi);
            break;
        case 14: // sys_rt_sigaction
            regs->rax = sys_rt_sigaction(regs->rdi, (const struct sigaction*)regs->rsi, (struct sigaction*)regs->rdx, regs->r10);
            break;
        case 15: // sys_rt_sigprocmask
            regs->rax = sys_rt_sigprocmask(regs->rdi, (const sigset_t*)regs->rsi, (sigset_t*)regs->rdx, regs->r10);
            break;
        case 16: // sys_ioctl
            regs->rax = sys_ioctl(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 17: // sys_pread64
            regs->rax = sys_pread64(regs->rdi, (void*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 18: // sys_pwrite64
            regs->rax = sys_pwrite64(regs->rdi, (void*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 19: // sys_readv
            regs->rax = sys_readv(regs->rdi, (const struct iovec*)regs->rsi, regs->rdx);
            break;
        case 20: // sys_writev
            regs->rax = sys_writev(regs->rdi, (const struct iovec*)regs->rsi, regs->rdx);
            break;
        case 21: // sys_access
            regs->rax = sys_access((char*)regs->rdi, regs->rsi);
            break;
        case 22: // sys_pipe
            regs->rax = sys_pipe((int*)regs->rdi);
            break;
        case 23: // sys_select
            regs->rax = sys_select(regs->rdi, (fd_set*)regs->rsi, (fd_set*)regs->rdx, (fd_set*)regs->r10, (struct timeval*)regs->r8);
            break;
        case 24: // sys_sched_yield
            regs->rax = sys_sched_yield();
            break;
        case 25: // sys_mremap
            regs->rax = sys_mremap((void*)regs->rdi, regs->rsi, regs->rdx, regs->r10, (void*)regs->r8);
            break;
        case 26: // sys_msync
            regs->rax = sys_msync((void*)regs->rdi, regs->rsi, regs->rdx);
            break;
        case 27: // sys_mincore
            regs->rax = sys_mincore((void*)regs->rdi, regs->rsi, (unsigned char*)regs->rdx);
            break;
        case 28: // sys_madvise
            regs->rax = sys_madvise((void*)regs->rdi, regs->rsi, regs->rdx);
            break;
        case 29: // sys_shmget
            regs->rax = sys_shmget(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 30: // sys_shmat
            regs->rax = sys_shmat(regs->rdi, (const void*)regs->rsi, regs->rdx);
            break;
        case 31: // sys_shmctl
            regs->rax = sys_shmctl(regs->rdi, regs->rsi, (struct shmid_ds*)regs->rdx);
            break;
        case 32: // sys_dup
            regs->rax = sys_dup(regs->rdi);
            break;
        case 33: // sys_dup2
            regs->rax = sys_dup2(regs->rdi, regs->rsi);
            break;
        case 34: // sys_pause
            regs->rax = sys_pause();
            break;
        case 35: // sys_nanosleep
            regs->rax = sys_nanosleep((const struct timespec*)regs->rdi, (struct timespec*)regs->rsi);
            break;
        case 36: // sys_getitimer
            regs->rax = sys_getitimer(regs->rdi, (struct itimerval*)regs->rsi);
            break;
        case 37: // sys_alarm
            regs->rax = sys_alarm(regs->rdi);
            break;
        case 38: // sys_setitimer
            regs->rax = sys_setitimer(regs->rdi, (const struct itimerval*)regs->rsi, (struct itimerval*)regs->rdx);
            break;
        case 39: // sys_getpid
            regs->rax = sys_getpid();
            break;
        case 40: // sys_sendfile
            regs->rax = sys_sendfile(regs->rdi, regs->rsi, (off_t*)regs->rdx, regs->r10);
            break;
        case 41: // sys_socket
            regs->rax = sys_socket(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 42: // sys_connect
            regs->rax = sys_connect(regs->rdi, (const struct sockaddr*)regs->rsi, regs->rdx);
            break;
        case 43: // sys_accept
            regs->rax = sys_accept(regs->rdi, (struct sockaddr*)regs->rsi, (socklen_t*)regs->rdx);
            break;
        case 44: // sys_sendto
            regs->rax = sys_sendto(regs->rdi, (const void*)regs->rsi, regs->rdx, regs->r10, (const struct sockaddr*)regs->r8, regs->r9);
            break;
        case 45: // sys_recvfrom
            regs->rax = sys_recvfrom(regs->rdi, (void*)regs->rsi, regs->rdx, regs->r10, (struct sockaddr*)regs->r8, (socklen_t*)regs->r9);
            break;
        case 46: // sys_sendmsg
            regs->rax = sys_sendmsg(regs->rdi, (const struct msghdr*)regs->rsi, regs->rdx);
            break;
        case 47: // sys_recvmsg
            regs->rax = sys_recvmsg(regs->rdi, (struct msghdr*)regs->rsi, regs->rdx);
            break;
        case 48: // sys_shutdown
            regs->rax = sys_shutdown(regs->rdi, regs->rsi);
            break;
        case 49: // sys_bind
            regs->rax = sys_bind(regs->rdi, (const struct sockaddr*)regs->rsi, regs->rdx);
            break;
        case 50: // sys_listen
            regs->rax = sys_listen(regs->rdi, regs->rsi);
            break;
        case 51: // sys_getsockname
            regs->rax = sys_getsockname(regs->rdi, (struct sockaddr*)regs->rsi, (socklen_t*)regs->rdx);
            break;
        case 52: // sys_getpeername
            regs->rax = sys_getpeername(regs->rdi, (struct sockaddr*)regs->rsi, (socklen_t*)regs->rdx);
            break;
        case 53: // sys_socketpair
            regs->rax = sys_socketpair(regs->rdi, regs->rsi, regs->rdx, (int*)regs->r10);
            break;
        case 54: // sys_setsockopt
            regs->rax = sys_setsockopt(regs->rdi, regs->rsi, regs->rdx, (const void*)regs->r10, regs->r8);
            break;
        case 55: // sys_getsockopt
            regs->rax = sys_getsockopt(regs->rdi, regs->rsi, regs->rdx, (void*)regs->r10, (socklen_t*)regs->r8);
            break;
        case 56: // sys_clone
            regs->rax = sys_clone(regs->rdi, (void*)regs->rsi, (int*)regs->rdx, (int*)regs->r10, (int*)regs->r8);
            break;
        case 57: // sys_fork
            regs->rax = sys_fork();
            break;
        case 58: // sys_vfork
            regs->rax = sys_vfork();
            break;
        case 59: // sys_execve
            regs->rax = sys_execve((char*)regs->rdi, (char**)regs->rsi, (char**)regs->rdx);
            break;
        case 60: // sys_exit_group
            regs->rax = sys_exit_group(regs->rdi);
            break;
        case 61: // sys_waitid
            regs->rax = sys_waitid(regs->rdi, regs->rsi, (struct siginfo*)regs->rdx, regs->r10, (struct rusage*)regs->r8);
            break;
        case 62: // sys_kill
            regs->rax = sys_kill(regs->rdi, regs->rsi);
            break;
        case 63: // sys_tkill
            regs->rax = sys_tkill(regs->rdi, regs->rsi);
            break;
        case 64: // sys_tgkill
            regs->rax = sys_tgkill(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 65: // sys_sigaltstack
            regs->rax = sys_sigaltstack((const stack_t*)regs->rdi, (stack_t*)regs->rsi);
            break;
        case 66: // sys_rt_sigsuspend
            regs->rax = sys_rt_sigsuspend((const sigset_t*)regs->rdi, regs->rsi);
            break;
        case 67: // sys_rt_sigpending
            regs->rax = sys_rt_sigpending((sigset_t*)regs->rsi, (sigset_t*)regs->rdx);
            break;
        case 68: // sys_rt_sigtimedwait
            regs->rax = sys_rt_sigtimedwait((const sigset_t*)regs->rdi, (siginfo_t*)regs->rsi, (const struct timespec*)regs->rdx, regs->r10);
            break;
        case 69: // sys_rt_sigqueueinfo
            regs->rax = sys_rt_sigqueueinfo(regs->rdi, regs->rsi, (const siginfo_t*)regs->rdx);
            break;
        case 70: // sys_sigwaitinfo
            regs->rax = sys_sigwaitinfo((const sigset_t*)regs->rdi, (siginfo_t*)regs->rsi);
            break;
        case 73: // sys_chown
            regs->rax = sys_chown((char*)regs->rdi, regs->rsi, regs->rdx);
            break;
        case 74: // sys_fchown
            regs->rax = sys_fchown(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 75: // sys_lchown
            regs->rax = sys_lchown((char*)regs->rdi, regs->rsi, regs->rdx);
            break;
        case 76: // sys_umask
            regs->rax = sys_umask(regs->rdi);
            break;
        case 77: // sys_gettimeofday
            regs->rax = sys_gettimeofday((struct timeval*)regs->rdi, (struct timezone*)regs->rsi);
            break;
        case 78: // sys_getrlimit
            regs->rax = sys_getrlimit(regs->rdi, (struct rlimit*)regs->rsi);
            break;
        case 79: // sys_getrusage
            regs->rax = sys_getrusage(regs->rdi, (struct rusage*)regs->rsi);
            break;
        case 80: // sys_sysinfo
            regs->rax = sys_sysinfo((struct sysinfo*)regs->rdi);
            break;
        case 81: // sys_times
            regs->rax = sys_times((struct tms*)regs->rsi);
            break;
        case 82: // sys_ptrace
            regs->rax = sys_ptrace(regs->rdi, regs->rsi, (void*)regs->rdx, (void*)regs->r10);
            break;
        case 83: // sys_getuid
            regs->rax = sys_getuid();
            break;
        case 84: // sys_syslog
            regs->rax = sys_syslog(regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 85: // sys_getgid
            regs->rax = sys_getgid();
            break;
        case 86: // sys_setuid
            regs->rax = sys_setuid(regs->rdi);
            break;
        case 87: // sys_setgid
            regs->rax = sys_setgid(regs->rdi);
            break;
        case 88: // sys_geteuid
            regs->rax = sys_geteuid();
            break;
        case 89: // sys_getegid
            regs->rax = sys_getegid();
            break;
        case 90: // sys_setpgid
            regs->rax = sys_setpgid(regs->rdi, regs->rsi);
            break;
        case 91: // sys_getppid
            regs->rax = sys_getppid();
            break;
        case 92: // sys_getpgrp
            regs->rax = sys_getpgrp();
            break;
        case 93: // sys_setsid
            regs->rax = sys_setsid();
            break;
        case 94: // sys_setreuid
            regs->rax = sys_setreuid(regs->rdi, regs->rsi);
            break;
        case 95: // sys_setregid
            regs->rax = sys_setregid(regs->rdi, regs->rsi);
            break;
        case 96: // sys_getgroups
            regs->rax = sys_getgroups(regs->rdi, (gid_t*)regs->rsi);
            break;
        case 97: // sys_setgroups
            regs->rax = sys_setgroups(regs->rdi, (const gid_t*)regs->rsi);
            break;
        case 98: // sys_setresuid
            regs->rax = sys_setresuid(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 99: // sys_getresuid
            regs->rax = sys_getresuid((uid_t*)regs->rdi, (uid_t*)regs->rsi, (uid_t*)regs->rdx);
            break;
        case 100: // sys_setresgid
            regs->rax = sys_setresgid(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 101: // sys_getresgid
            regs->rax = sys_getresgid((gid_t*)regs->rdi, (gid_t*)regs->rsi, (gid_t*)regs->rdx);
            break;
        case 102: // sys_getpgid
            regs->rax = sys_getpgid(regs->rdi);
            break;
        case 103: // sys_setfsuid
            regs->rax = sys_setfsuid(regs->rdi);
            break;
        case 104: // sys_setfsgid
            regs->rax = sys_setfsgid(regs->rdi);
            break;
        case 105: // sys_getsid
            regs->rax = sys_getsid(regs->rdi);
            break;
        case 106: // sys_capget
            regs->rax = sys_capget((cap_user_header_t)regs->rdi, (cap_user_data_t)regs->rsi);
            break;
        case 107: // sys_capset
            regs->rax = sys_capset((cap_user_header_t)regs->rdi, (const cap_user_data_t)regs->rsi);
            break;
        case 108: // sys_rt_sigreturn
            regs->rax = sys_rt_sigreturn();
            break;
        case 109: // sys_setpriority
            regs->rax = sys_setpriority(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 110: // sys_getpriority
            regs->rax = sys_getpriority(regs->rdi, regs->rsi);
            break;
        case 111: // sys_sched_setparam
            regs->rax = sys_sched_setparam(regs->rdi, (const struct sched_param*)regs->rsi);
            break;
        case 112: // sys_sched_getparam
            regs->rax = sys_sched_getparam(regs->rdi, (struct sched_param*)regs->rsi);
            break;
        case 113: // sys_sched_setscheduler
            regs->rax = sys_sched_setscheduler(regs->rdi, regs->rsi, (const struct sched_param*)regs->rdx);
            break;
        case 114: // sys_sched_getscheduler
            regs->rax = sys_sched_getscheduler(regs->rdi);
            break;
        case 115: // sys_sched_get_priority_max
            regs->rax = sys_sched_get_priority_max(regs->rdi);
            break;
        case 116: // sys_sched_get_priority_min
            regs->rax = sys_sched_get_priority_min(regs->rdi);
            break;
        case 117: // sys_sched_rr_get_interval
            regs->rax = sys_sched_rr_get_interval(regs->rdi, (struct timespec*)regs->rsi);
            break;
        case 118: // sys_mlock
            regs->rax = sys_mlock((const void*)regs->rdi, regs->rsi);
            break;
        case 119: // sys_munlock
            regs->rax = sys_munlock((const void*)regs->rdi, regs->rsi);
            break;
        case 120: // sys_mlockall
            regs->rax = sys_mlockall(regs->rdi);
            break;
        case 121: // sys_munlockall
            regs->rax = sys_munlockall();
            break;
        case 122: // sys_vhangup
            regs->rax = sys_vhangup();
            break;
        case 123: // sys_modify_ldt
            regs->rax = sys_modify_ldt(regs->rdi, (void*)regs->rsi, regs->rdx);
            break;
        case 124: // sys_pivot_root
            regs->rax = sys_pivot_root((char*)regs->rdi, (char*)regs->rsi);
            break;
        case 125: // sys__sysctl
            regs->rax = sys__sysctl((struct __sysctl_args*)regs->rdi);
            break;
        case 126: // sys_prctl
            regs->rax = sys_prctl(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);
            break;
        case 127: // sys_arch_prctl
            regs->rax = sys_arch_prctl(regs->rdi, regs->rsi);
            break;
        case 128: // sys_adjtimex
            regs->rax = sys_adjtimex((struct timex*)regs->rdi);
            break;
        case 129: // sys_setrlimit
            regs->rax = sys_setrlimit(regs->rdi, (const struct rlimit*)regs->rsi);
            break;
        case 130: // sys_chroot
            regs->rax = sys_chroot((char*)regs->rdi);
            break;
        case 131: // sys_sync
            regs->rax = sys_sync();
            break;
        case 132: // sys_acct
            regs->rax = sys_acct((char*)regs->rdi);
            break;
        case 133: // sys_settimeofday
            regs->rax = sys_settimeofday((struct timeval*)regs->rdi, (struct timezone*)regs->rsi);
            break;
        case 134: // sys_mount
            regs->rax = sys_mount((char*)regs->rdi, (char*)regs->rsi, (char*)regs->rdx, regs->r10, (void*)regs->r8);
            break;
        case 135: // sys_umount2
            regs->rax = sys_umount2((char*)regs->rdi, regs->rsi);
            break;
        case 136: // sys_swapon
            regs->rax = sys_swapon((char*)regs->rdi, regs->rsi);
            break;
        case 137: // sys_swapoff
            regs->rax = sys_swapoff((char*)regs->rdi);
            break;
        case 138: // sys_reboot
            regs->rax = sys_reboot(regs->rdi, regs->rsi, regs->rdx, (void*)regs->r10);
            break;
        case 139: // sys_sethostname
            regs->rax = sys_sethostname((char*)regs->rdi, regs->rsi);
            break;
        case 140: // sys_setdomainname
            regs->rax = sys_setdomainname((char*)regs->rdi, regs->rsi);
            break;
        case 141: // sys_iopl
            regs->rax = sys_iopl(regs->rdi);
            break;
        case 142: // sys_ioperm
            regs->rax = sys_ioperm(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 143: // sys_create_module
            regs->rax = sys_create_module((char*)regs->rdi, regs->rsi);
            break;
        case 144: // sys_init_module
            regs->rax = sys_init_module((void*)regs->rdi, regs->rsi, (char*)regs->rdx);
            break;
        case 145: // sys_delete_module
            regs->rax = sys_delete_module((char*)regs->rdi, regs->rsi);
            break;
        case 146: // sys_get_kernel_syms
            regs->rax = sys_get_kernel_syms((struct kernel_sym*)regs->rdi);
            break;
        case 147: // sys_query_module
            regs->rax = sys_query_module((char*)regs->rdi, regs->rsi, (void*)regs->rdx, regs->r10, (size_t*)regs->r8);
            break;
        case 148: // sys_quotactl
            regs->rax = sys_quotactl(regs->rdi, (char*)regs->rdi, regs->rsi, regs->rdx, (void*)regs->r10);
            break;
        case 149: // sys_nfsservctl
            regs->rax = sys_nfsservctl(regs->rdi, (struct nfsctl_arg*)regs->rsi, (void*)regs->rdx);
            break;
        case 150: // sys_getpmsg
            regs->rax = sys_getpmsg((void*)regs->rdi, (void*)regs->rsi, (void*)regs->rdx, (void*)regs->r10, (void*)regs->r8);
            break;
        case 151: // sys_putpmsg
            regs->rax = sys_putpmsg((void*)regs->rdi, (void*)regs->rsi, (void*)regs->rdx, (void*)regs->r10, (void*)regs->r8);
            break;
        case 152: // sys_afs_syscall
            regs->rax = sys_afs_syscall((void*)regs->rdi, (void*)regs->rsi, (void*)regs->rdx, (void*)regs->r10);
            break;
        case 153: // sys_tuxcall
            regs->rax = sys_tuxcall((void*)regs->rdi, (void*)regs->rsi);
            break;
        case 154: // sys_security
            regs->rax = sys_security((int)regs->rdi, (void*)regs->rsi, (void*)regs->rdx);
            break;
        case 155: // sys_gettid
            regs->rax = sys_gettid();
            break;
        case 156: // sys_readahead
            regs->rax = sys_readahead(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 157: // sys_setxattr
            regs->rax = sys_setxattr((char*)regs->rdi, (char*)regs->rsi, (const void*)regs->rdx, regs->r10, regs->r8);
            break;
        case 158: // sys_lsetxattr
            regs->rax = sys_lsetxattr((char*)regs->rdi, (char*)regs->rsi, (const void*)regs->rdx, regs->r10, regs->r8);
            break;
        case 159: // sys_fsetxattr
            regs->rax = sys_fsetxattr(regs->rdi, (char*)regs->rsi, (const void*)regs->rdx, regs->r10, regs->r8);
            break;
        case 160: // sys_getxattr
            regs->rax = sys_getxattr((char*)regs->rdi, (char*)regs->rsi, (void*)regs->rdx, regs->r10);
            break;
        case 161: // sys_lgetxattr
            regs->rax = sys_lgetxattr((char*)regs->rdi, (char*)regs->rsi, (void*)regs->rdx, regs->r10);
            break;
        case 162: // sys_fgetxattr
            regs->rax = sys_fgetxattr(regs->rdi, (char*)regs->rsi, (void*)regs->rdx, regs->r10);
            break;
        case 163: // sys_listxattr
            regs->rax = sys_listxattr((char*)regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 164: // sys_llistxattr
            regs->rax = sys_llistxattr((char*)regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 165: // sys_flistxattr
            regs->rax = sys_flistxattr(regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 166: // sys_removexattr
            regs->rax = sys_removexattr((char*)regs->rdi, (char*)regs->rsi);
            break;
        case 167: // sys_lremovexattr
            regs->rax = sys_lremovexattr((char*)regs->rdi, (char*)regs->rsi);
            break;
        case 168: // sys_fremovexattr
            regs->rax = sys_fremovexattr(regs->rdi, (char*)regs->rsi);
            break;
        case 170: // sys_time
            regs->rax = sys_time((time_t*)regs->rdi);
            break;
        case 171: // sys_futex
            regs->rax = sys_futex((int*)regs->rdi, regs->rsi, regs->rdx, (const struct timespec*)regs->r10, (int*)regs->r8, regs->r9);
            break;
        case 172: // sys_sched_setaffinity
            regs->rax = sys_sched_setaffinity(regs->rdi, regs->rsi, (const unsigned long*)regs->rdx);
            break;
        case 173: // sys_sched_getaffinity
            regs->rax = sys_sched_getaffinity(regs->rdi, regs->rsi, (unsigned long*)regs->rdx);
            break;
        case 174: // sys_set_thread_area
            regs->rax = sys_set_thread_area((struct user_desc*)regs->rdi);
            break;
        case 175: // sys_io_setup
            regs->rax = sys_io_setup(regs->rdi, (struct aio_context_t*)regs->rsi);
            break;
        case 176: // sys_io_destroy
            regs->rax = sys_io_destroy(regs->rdi);
            break;
        case 177: // sys_io_getevents
            regs->rax = sys_io_getevents(regs->rdi, regs->rsi, regs->rdx, (struct io_event*)regs->r10, (struct timespec*)regs->r8);
            break;
        case 178: // sys_io_submit
            regs->rax = sys_io_submit(regs->rdi, regs->rsi, (struct iocb**)regs->rdx);
            break;
        case 179: // sys_io_cancel
            regs->rax = sys_io_cancel(regs->rdi, (struct iocb*)regs->rsi, (struct io_event*)regs->rdx);
            break;
        case 180: // sys_get_thread_area
            regs->rax = sys_get_thread_area((struct user_desc*)regs->rdi);
            break;
        case 181: // sys_lookup_dcookie
            regs->rax = sys_lookup_dcookie(regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 182: // sys_epoll_create
            regs->rax = sys_epoll_create(regs->rdi);
            break;
        case 183: // sys_epoll_ctl_old
            regs->rax = sys_epoll_ctl_old(regs->rdi, regs->rsi, regs->rdx, (void*)regs->r10);
            break;
        case 184: // sys_epoll_wait_old
            regs->rax = sys_epoll_wait_old(regs->rdi, (void*)regs->rsi, (void*)regs->rdx, regs->r10);
            break;
        case 185: // sys_remap_file_pages
            regs->rax = sys_remap_file_pages((void*)regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);
            break;
        case 186: // sys_getdents64
            regs->rax = sys_getdents64(regs->rdi, (struct dirent*)regs->rsi, regs->rdx);
            break;
        case 187: // sys_set_tid_address
            regs->rax = sys_set_tid_address((int*)regs->rdi);
            break;
        case 188: // sys_restart_syscall
            regs->rax = sys_restart_syscall();
            break;
        case 189: // sys_semtimedop
            regs->rax = sys_semtimedop(regs->rdi, (struct->rsi, regs->rdx, (const struct timespec*)regs->r10);
            break;
        case 190: // sys_fadvise64
            regs->rax = sys_fadvise64(regs->rdi, regs->rsi, regs->rdx, regs->r10);
            break;
        case 191: // sys_timer_create
            regs->rax = sys_timer_create(regs->rdi, (struct sigevent*)regs->rsi, (timer_t*)regs-> break;
        case 192: // sys_timer_settime
            regs->rax = sys_timer_settime(regs->rdi, regs->rsi, (const struct itimerspec*)regs->rdx, (struct itimerspec*)regs->r8);
            break;
        case 193: // sys_timer_gettime
            regs->rax = sys_timer_gettime(regs->rdi, (struct itimerspec*)regs->rsi);
            break;
        case 194: // sys_timer_getoverrun
            regs->rax = sys_timer_getoverrun(regs->rdi);
            break;
        case 195: // sys_timer_delete
            regs->rax = sys_timer_delete(regs->rdi);
            break;
        case 196: // sys_clock_settime
            regs->rax = sys_clock_settime(regs->rdi, (const struct timespec*)regs->rsi);
            break;
        case 197: // sys_clock_gettime
            regs->rax = sys_clock_gettime(regs->rdi, (struct timespec*)regs->rsi);
            break;
        case 198: // sys_clock_getres
            regs->rax = sys_clock_getres(regs->rdi, (struct timespec*)regs->rsi);
            break;
        case 199: // sys_clock_nanosleep
            regs->rax = sys_clock_nanosleep(regs->rdi, regs->rsi, (const struct timespec*)regs->rdx, (struct timespec*)regs->r10);
            break;
        case 200: // sys_exit_group
            regs->rax = sys_exit_group(regs->rdi);
            break;
        case 201: // sys_epoll_wait
            regs->rax = sys_epoll_wait(regs->rdi, (struct epoll_event*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 202: // sys_epoll_ctl
            regs->rax = sys_epoll_ctl(regs->rdi, regs->rsi, regs->rdx, (struct epoll_event*)regs->r10);
            break;
        case 203: // sys_tgkill
            regs->rax = sys_tgkill(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 204: // sys_utimes
            regs->rax = sys_utimes((char*)regs->rdi, (const struct timeval*)regs->rsi);
            break;
        case 205: // sys_vserver
            regs->rax = sys_vserver(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
            break;
        case 206: // sys_mbind
            regs->rax = sys_mbind((void*)regs->rdi, regs->rsi, regs->rdx, (const unsigned long*)regs->r10, regs->r8, regs->r9);
            break;
        case 207: // sys_set_mempolicy
            regs->rax = sys_set_mempolicy(regs->rdi, (const unsigned long*)regs->rsi, regs->rdx);
            break;
        case 208: // sys_get_mempolicy
            regs->rax = sys_get_mempolicy((int*)regs->rdi, (unsigned long*)regs->rsi, regs->rdx, (void*)regs->r10, regs->r8);
            break;
        case 209: // sys_mq_open
            regs->rax = sys_mq_open((char*)regs->rdi, regs->rsi, regs->rdx, (struct mq_attr*)regs->r10);
            break;
        case 210: // sys_mq_unlink
            regs->rax = sys_mq_unlink((char*)regs->rdi);
            break;
        case 211: // sys_mq_timedsend
            regs->rax = sys_mq_timedsend(regs->rdi, (char*)regs->rsi, regs->rdx, regs->r10, (const struct timespec*)regs->r8);
            break;
        case 212: // sys_mq_timedreceive
            regs->rax = sys_mq_timedreceive(regs->rdi, (char*)regs->rsi, regs->rdx, (unsigned int*)regs->r10, (const struct timespec*)regs->r8);
            break;
        case 213: // sys_mq_notify
            regs->rax = sys_mq_notify(regs->rdi, (const struct sigevent*)regs->rsi);
            break;
        case 214: // sys_mq_getsetattr
            regs->rax = sys_mq_getsetattr(regs->rdi, (const struct mq_attr*)regs->rsi, (struct mq_attr*)regs->rdx);
            break;
        case 215: // sys_kexec_load
            regs->rax = sys_kexec_load(regs->rdi, regs->rsi, (const struct kexec_segment*)regs->rdx, regs->r10);
            break;
        case 216: // sys_waitid
            regs->rax = sys_waitid(regs->rdi, regs->rsi, (struct siginfo*)regs->rdx, regs->r10, (struct rusage*)regs->r8);
            break;
        case 217: // sys_add_key
            regs->rax = sys_add_key((char*)regs->rdi, (char*)regs->rsi, (const void*)regs->rdx, regs->r10, regs->r8);
            break;
        case 218: // sys_request_key
            regs->rax = sys_request_key((char*)regs->rdi, (char*)regs->rsi, (char*)regs->rdx, regs->r10);
            break;
        case 219: // sys_keyctl
            regs->rax = sys_keyctl(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);
            break;
        case 220: // sys_ioprio_set
            regs->rax = sys_ioprio_set(regs->rdi, regs->rsi, regs->rdx);
            break;
        case 221: // sys_ioprio_get
            regs->rax = sys_ioprio_get(regs->rdi, regs->rsi);
            break;
        case 222: // sys_inotify_init
            regs->rax = sys_inotify_init();
            break;
        case 223: // sys_inotify_add_watch
            regs->rax = sys_inotify_add_watch(regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 224: // sys_inotify_rm_watch
            regs->rax = sys_inotify_rm_watch(regs->rdi, regs->rsi);
            break;
        case 225: // sys_migrate_pages
            regs->rax = sys_migrate_pages(regs->rdi, regs->rsi, (const unsigned long*)regs->rdx, (const unsigned long*)regs->r10);
            break;
        case 226: // sys_openat
            regs->rax = sys_openat(regs->rdi, (char*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 227: // sys_mkdirat
            regs->rax = sys_mkdirat(regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 228: // sys_mknodat
            regs->rax = sys_mknodat(regs->rdi, (char*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 229: // sys_fchownat
            regs->rax = sys_fchownat(regs->rdi, (char*)regs->rsi, regs->rdx, regs->r10, regs->r8);
            break;
        case 230: // sys_futimesat
            regs->rax = sys_futimesat(regs->rdi, (char*)regs->rsi, (const struct timeval*)regs->rdx);
            break;
        case 231: // sys_newfstatat
            regs->rax = sys_newfstatat(regs->rdi, (char*)regs->rsi, (struct stat*)regs->rdx, regs->r10);
            break;
        case 232: // sys_unlinkat
            regs->rax = sys_unlinkat(regs->rdi, (char*)regs->rsi, regs->rdx);
            break;
        case 233: // sys_renameat
            regs->rax = sys_renameat(regs->rdi, (char*)regs->rsi, regs->rdx, (char*)regs->r10);
            break;
        case 234: // sys_linkat
            regs->rax = sys_linkat(regs->rdi, (char*)regs->rsi, (char*)regs->rdx, (char*)regs->r10, regs->r8);
            break;
        case 235: // sys_symlinkat
            regs->rax = sys_symlinkat((char*)regs->rdi, regs->rsi, (char*)regs->rdx);
            break;
        case 236: // sys_readlinkat
            regs->rax = sys_readlinkat(regs->rdi, (char*)regs->rsi, (char*)regs->rdx, regs->r10);
            break;
        case 237: // sys_fchmodat
            regs->rax = sys_fchmodat(regs->rdi, (char*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 238: // sys_faccessat
            regs->rax = sys_faccessat(regs->rdi, (char*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 239: // sys_pselect6
            regs->rax = sys_pselect6(regs->rdi, (fd_set*)regs->rsi, (fd_set*)regs->rdx, (fd_set*)regs->r10, (struct timespec*)regs->r8, (void*)regs->r9);
            break;
        case 240: // sys_ppoll
            regs->rax = sys_ppoll((struct pollfd*)regs->rdi, regs->rsi, (const struct timespec*)regs->rdx, (const sigset_t*)regs->r10, regs->r8);
            break;
        case 241: // sys_unshare
            regs->rax = sys_unshare(regs->rdi);
            break;
        case 242: // sys_set_robust_list
            regs->rax = sys_set_robust_list((struct robust_list_head*)regs->rdi, regs->rsi);
            break;
        case 243: // sys_get_robust_list
            regs->rax = sys_get_robust_list(regs->rdi, (struct robust_list_head**)regs->rsi, (size_t*)regs->rdx);
            break;
        case 244: // sys_splice
            regs->rax = sys_splice(regs->rdi, (loff_t*)regs->rsi, regs->rdx, (loff_t*)regs->r10, regs->r8, regs->r9);
            break;
        case 245: // sys_tee
            regs->rax = sys_tee(regs->rdi, regs->rsi, regs->rdx, regs->r10);
            break;
        case 246: // sys_sync_file_range
            sys_sync_file_range(regs->rdi, regs->rsi, regs->rdx, regs->r10);
            break;
        case 247: // sys_vmsplice
            regs->rax = sys_vmsplice(regs->rdi, (const struct iovec*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 248: // sys_move_pages
            regs->rax = sys_move_pages(regs->rdi, regs->rsi, (void**)regs->rdx, (const int*)regs->r10, (int*)regs->r8, regs->r9);
            break;
        case 249: // sys_utimensat
            regs->rax = sys_utimensat(regs->rdi, (char*)regs->rsi, (const struct timespec*)regs->rdx, regs->r10);
            break;
        case 250: // sys_epoll_pwait
            regs->rax = sys_epoll_pwait(regs->rdi, (struct epoll_event*)regs->rsi, regs->rdx, regs->r10, (const sigset_t*)regs->r8, regs->r9);
            break;
        case 251: // sys_signalfd
            regs->rax = sys_signalfd(regs->rdi, (const sigset_t*)regs->rsi, regs->rdx, regs->r10);
            break;
        case 252: // sys_timerfd_create
            regs->rax = sys_timerfd_create(regs->rdi, regs->rsi);
            break;
        case 253: // sys_eventfd
            regs->rax = sys_eventfd(regs->rdi, regs->rsi);
            break;
        case 254: // sys_fallocate
            regs->rax = sys_fallocate(regs->rdi, regs->rsi, regs->rdx, regs->r10);
            break;
        case 255: // sys_timerfd_settime
            regs->rax = sys_timerfd_settime(regs->rdi, regs->rsi, (const struct itimerspec*)regs->rdx, (struct itimerspec*)regs->r10);
            break;
        case 256: // sys_timerfd_gettime
            regs->rax = sys_timerfd_gettime(regs->rdi, (struct itimerspec*)regs->rsi);
            break;
        case 1000: // Custom network send syscall
            regs->rax = net_send((void*)regs->rdi, regs->rsi);
            break;
        case 1001: // Custom network receive syscall
            regs->rax = net_receive((void*)regs->rdi, regs->rsi);
            break;
        default:
            printf("Unknown syscall: %d\n", regs->rax);
            regs->rax = -1;
            break;
    }
}

// System call implementations
int sys_exit(int status) {
    current_process->state = PROC_ZOMBIE;
    current_process->exit_code = status;
    schedule();
    return 0;
}

int sys_read(int fd, void *buf, size_t count) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    
    struct file_descriptor *file = current_process->open_files[fd];
    if(file->type == 0) {
        uint8_t *data = (uint8_t*)file->data;
        size_t bytes_to_read = count;
        if(file->offset + count > file->size) {
            bytes_to_read = file->size - file->offset;
        }
        if(bytes_to_read > 0) {
            memcpy(buf, data + file->offset, bytes_to_read);
            file->offset += bytes_to_read;
        }
        return bytes_to_read;
    }
    return -1;
}

int sys_write(int fd, const void *buf, size_t count) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    
    struct file_descriptor *file = current_process->open_files[fd];
    if(file->type == 0) {
        uint8_t *data = (uint8_t*)file->data;
        if(file->offset + count > file->size) {
            // Extend file if needed
            if(file->offset + count > 4096) { // Assuming 4KB file limit for this simple impl
                return -1; // File too large
            }
            file->size = file->offset + count;
        }
        memcpy(data + file->offset, buf, count);
        file->offset += count;
        return count;
    }
    return -1;
}

int sys_open(const char *pathname, int flags, mode_t mode) {
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(!current_process->open_files[i]) {
            current_process->open_files[i] = (struct file_descriptor*)get_free_page();
            if(!current_process->open_files[i]) {
                return -1;
            }
            current_process->open_files[i]->in_use = 1;
            current_process->open_files[i]->type = 0;
            current_process->open_files[i]->data = get_free_page();
            if(!current_process->open_files[i]->data) {
                free_page((paddr_t)current_process->open_files[i]);
                current_process->open_files[i] = NULL;
                return -1;
            }
            current_process->open_files[i]->flags = flags;
            current_process->open_files[i]->offset = 0;
            current_process->open_files[i]->size = 0;
            return i;
        }
    }
    return -1;
}

int sys_close(int fd) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    
    if(current_process->open_files[fd]->data) {
        free_page((paddr_t)current_process->open_files[fd]->data);
    }
    free_page((paddr_t)current_process->open_files[fd]);
    current_process->open_files[fd] = NULL;
    return 0;
}

int sys_stat(const char *pathname, struct stat *statbuf) {
    if(!statbuf) return -1;
    // Simplified implementation
    memset(statbuf, 0, sizeof(struct stat));
    statbuf->st_dev = 1;
    statbuf->st_ino = 1;
    statbuf->st_mode = 0755 | S_IFREG; // Regular file
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_size = 1024;
    statbuf->st_blksize = 512;
    statbuf->st_blocks = 2;
    return 0;
}

int sys_fstat(int fd, struct stat *statbuf) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd] || !statbuf) return -1;
    return sys_stat("", statbuf);
}

int sys_lstat(const char *pathname, struct stat *statbuf) {
    return sys_stat(pathname, statbuf);
}

int sys_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    if(!fds) return -1;
    // Simplified implementation - assume all are ready immediately
    int ready = 0;
    for(nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = fds[i].events & (POLLIN | POLLOUT | POLLERR); // Basic event simulation
        if(fds[i].revents) ready++;
    }
    return ready;
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    
    struct file_descriptor *file = current_process->open_files off_t new_offset;
    
    switch(whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = file->offset + offset;
            break;
        case SEEK_END:
            new_offset = file->size + offset; // Use file size instead of hardcoded value
            break;
        default:
            return -1;
    }
    
    if(new_offset < 0) return -1;
    
    file->offset = new_offset;
    return file->offset;
}

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if(length == 0) return (void*)-1;
    
    // Find a suitable address if none provided
    if(addr == NULL) {
        addr = (void*)current_process->heap_brk;
        current_process->heap_brk += (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    uint32_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    for(uint32_t i = 0; i < pages; i++) {
        paddr_t paddr = get_free_page();
        if(!paddr) {
            // Failed to allocate - unmap previously allocated pages
            for(uint32_t j = 0; j < i; j++) {
                paddr_t prev_paddr = get_physical_address(current_process->page_dir, (vaddr_t)addr + j * PAGE_SIZE);
                if(prev_paddr) {
                    free_page(prev_paddr);
                }
            }
            return (void*)-1;
        }
        
        int pte_flags = 1; // Present
        if(prot & PROT_WRITE) pte_flags |= 2; // Writable
        if(prot & PROT_EXEC) pte_flags |= 4; // Executable
        if(flags & MAP_PRIVATE) pte_flags |= 16; // User accessible
        
        map_page(current_process->page_dir, (vaddr_t)addr + i * PAGE_SIZE, paddr, pte_flags);
    }
    
    return addr;
}

int sys_mprotect(void *addr, size_t len, int prot) {
    if(!addr || len == 0) return -1;
    
    vaddr_t start = (vaddr_t)addr;
    uint32_t pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for(uint32_t i = 0; i < pages; i++) {
        struct page_table_entry *pte = get_pte(current_process->page_dir, start + i * PAGE_SIZE);
        if(pte && pte->present) {
            pte->writable = (prot & PROT_WRITE) ? 1 : 0;
            pte->user = 1; // Always user accessible for now
            pte->nx = (prot & PROT_EXEC) ? 0 : 1; // NX bit: 0 = executable, 1 = non-executable
        }
    }
    
    return 0;
}

int sys_munmap(void *addr, size_t length) {
    if(!addr || length == 0) return -1;
    
    vaddr_t start = (vaddr_t)addr;
    uint32_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for(uint32_t i = 0; i < pages; i++) {
        paddr_t paddr = get_physical_address(current_process->page_dir, start + i * PAGE_SIZE);
        if(paddr) {
            free_page(paddr);
            unmap_page(current_process->page_dir, start + i * PAGE_SIZE);
        }
    }
    
    return 0;
}

void *sys_brk(void *addr) {
    if(!addr) return (void*)current_process->heap_brk;
    
    vaddr_t new_brk = (vaddr_t)addr;
    if(new_brk >= current_process->heap_start) {
        if(new_brk <= current_process->heap_end + 0x1000000) { // 16MB heap limit
            // If growing, make sure we have enough pages
            if(new_brk > current_process->heap_brk) {
                vaddr_t old_brk = current_process->heap_brk;
                uint32_t pages_needed = (new_brk - old_brk + PAGE_SIZE - 1) / PAGE_SIZE;
                
                for(uint32_t i = 0; i < pages_needed; i++) {
                    paddr_t page = get_free_page();
                    if(!page) {
                        // Roll back allocation on failure
                        vaddr_t rollback_brk = old_brk + i * PAGE_SIZE;
                        for(vaddr_t rollback_addr = old_brk; rollback_addr < rollback_brk; rollback_addr += PAGE_SIZE) {
                            paddr_t rollback_page = get_physical_address(current_process->page_dir, rollback_addr);
                            if(rollback_page) {
                                free_page(rollback_page);
                                unmap_page(current_process->page_dir, rollback_addr);
                            }
                        }
                        return (void*)current_process->heap_brk;
                    }
                    
                    map_page(current_process->page_dir, old_brk + i * PAGE_SIZE, page, 3); // RW user
                }
            }
            
            current_process->heap_brk = new_brk;
            return addr;
        }
    }
    return (void*)current_process->heap_brk;
}

int sys_rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact, size_t sigsetsize) {
    if(signum < 0 || signum >= 64) return -1; // Signal range check
    
    if(oldact) {
        // Copy old action if requested
        if(current_process->signal_handlers[signum]) {
            // Return current handler info
            oldact->sa_handler = current_process->signal_handlers[signum];
            oldact->sa_flags = 0; // Default flags
            // sa_mask not implemented
        } else {
            oldact->sa_handler = 0; // SIG_DFL
            oldact->sa_flags = 0;
            // sa_mask not implemented
        }
    }
    
    if(act) {
        // Install new handler
        current_process->signal_handlers[signum] = act->sa_handler;
    }
    
    return 0;
}

int sys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize) {
    if(oldset) {
        *oldset = current_process->blocked_signals;
    }
    
    if(set) {
        switch(how) {
            case SIG_BLOCK:
                current_process->blocked_signals |= *set;
                break;
            case SIG_UNBLOCK:
                current_process->blocked_signals &= ~(*set);
                break;
            case SIG_SETMASK:
                current_process->blocked_signals = *set;
                break;
            default:
                return -1;
        }
    }
    
    return 0;
}

int sys_ioctl(int fd, unsigned long request, ...) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    
    // Handle terminal/ioctl requests
    struct file_descriptor *file = current_process->open_files[fd];
    
    switch(request) {
        case 0x5401: // TCGETS
            // Get terminal attributes - simplified
            return 0;
        case 0x5402: // TCSETS
            // Set terminal attributes - simplified
            return 0;
        case 0x5421: // FIONBIO
            // Non-blocking I/O - simplified
            return 0;
        default:
            return -1;
    }
}

ssize_t sys_pread64(int fd, void *buf, size_t count, off_t offset) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    
    struct file_descriptor *file = current_process->open_files[fd];
    off_t old_offset = file->offset;
    file->offset = offset;
    ssize_t result = sys_read(fd, buf, count);
    file->offset = old_offset;
    return result;
}

ssize_t sys_pwrite64(int fd, const void *buf, size_t count, off_t offset) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    
    struct file_descriptor *file = current_process->open_files[fd];
    off_t old_offset = file->offset;
    file->offset = offset;
    ssize_t result = sys_write(fd, buf, count);
    file->offset = old_offset;
    return result;
}

ssize_t sys_readv(int fd, const struct iovec *iov, int iovcnt) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd] || !iov || iovcnt <= 0) return -1;
    
    ssize_t total = 0;
    for(int i = 0; i < iovcnt; i++) {
        if(iov[i].iov_base == NULL || iov[i].iov_len == 0) continue;
        
        ssize_t result = sys_read(fd, iov[i].iov_base, iov[i].iov_len);
        if(result < 0) {
            if(total == 0) return result;
            break;
        }
        total += result;
        if(result < (ssize_t)iov[i].iov_len) break; // EOF or partial read
    }
    return total;
}

ssize_t sys_writev(int fd, const struct iovec *iov, int iovcnt) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd] || !iov || iovcnt <= 0) return -1;
    
    ssize_t total = 0;
    for(int i = 0; i < iovcnt; i++) {
        if(iov[i].iov_base == NULL || iov[i].iov_len == 0) continue;
        
        ssize_t result = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
        if(result < 0) {
            if(total == 0) return result;
            break;
        }
        total += result;
        if(result < (ssize_t)iov[i].iov_len) break; // Partial write
    }
    return total;
}

int sys_access(const char *pathname, int mode) {
    if(!pathname) return -1;
    
    // For simplicity, assume all files exist and have proper permissions
    return 0;
}

int sys_pipe(int pipefd[2]) {
    if(!pipefd) return -1;
    
    for(int i = 0; i < 2; i++) {
        int fd = -1;
        for(int j = 0; j < MAX_OPEN_FILES; j++) {
            if(!current_process->open_files[j]) {
                fd = j;
                break;
            }
        }
        if(fd == -1) {
            // If we opened one pipe end but can't open the second, close the first
            if(i == 1 && current_process->open_files[pipefd[0]]) {
                sys_close(pipefd[0]);
            }
            return -1;
        }
        
        current_process->open_files[fd] = (struct file_descriptor*)get_free_page();
        if(!current_process->open_files[fd]) {
            // Close any already opened pipe ends
            for(int k = 0; k < i; k++) {
                sys_close(pipefd[k]);
            }
            return -1;
        }
        current_process->open_files[fd]->in_use = 1;
        current_process->open_files[fd]->type = 1; // Pipe
        current_process->open_files[fd]->data = get_free_page(); // Buffer for pipe
        if(!current_process->open_files[fd]->data) {
            free_page((paddr_t)current_process->open_files[fd]);
            current_process->open_files[fd] = NULL;
            // Close any already opened pipe ends
            for(int k = 0; k < i; k++) {
                sys_close(pipefd[k]);
            }
            return -1;
        }
        current_process->open_files[fd]->flags = (i == 0) ? 0 : 1; // O_RDONLY or O_WRONLY
        current_process->open_files[fd]->offset = 0;
        current_process->open_files[fd]->size = 0;
        pipefd[i] = fd;
    }
    return 0;
}

int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    int count = 0;
    
    if(readfds) {
        for(int i = 0; i < nfds; i++) {
            if(i < FD_SETSIZE && readfds->__fds_bits[i/64] & (1UL << (i%64))) {
                // For simplicity, assume all readable
                count++;
            }
        }
    }
    
    if(writefds) {
        for(int i = 0; i < nfds; i++) {
            if(i < FD_SETSIZE && writefds->__fds_bits[i/64] & (1UL << (i%64))) {
                // For simplicity, assume all writable
                count++;
            }
        }
    }
    
    if(exceptfds) {
        for(int i = 0; i < nfds; i++) {
            if(i < FD_SETSIZE && exceptfds->__fds_bits[i/64] & (1UL << (i%64))) {
                // No exceptional conditions
            }
        }
    }
    
    return count;
}

int sys_sched_yield(void) {
    current_process->state = PROC_READY;
    schedule();
    return 0;
}

void *sys_mremap(void *old_address, size_t old_size, size_t new_size, int flags, void *new_address) {
    if(!old_address || old_size == 0 || new_size == 0) return (void*)-1;
    
    if(flags & 0x2) { // MREMAP_FIXED
        if(!new_address) return (void*)-1;
        
        // Unmap old region
        sys_munmap(old_address, old_size);
        
        // Map new region
        return sys_mmap(new_address, new_size, 3, 0, -1, 0); // RW, private
    } else {
        if(new_size <= old_size) {
            // Shrinking - just unmap the excess
            size_t excess = old_size - new_size;
            if(excess > 0) {
                sys_munmap((char*)old_address + new_size, excess);
            }
            return old_address;
        } else {
            // Growing - allocate new region and copy
            void *new_addr = sys_mmap(NULL, new_size, 3, 0, -1, 0); // RW, private
            if(new_addr == (void*)-1) return (void*)-1;
            
            memcpy(new_addr, old_address, old_size);
            sys_munmap(old_address, old_size);
            return new_addr;
        }
    }
}

int sys_msync(void *addr, size_t length, int flags) {
    if(!addr || length == 0) return -1;
    // For now, treat as successful since we don't have disk-backed memory
    return 0;
}

int sys_mincore(void *addr, size_t length, unsigned char *vec) {
    if(!addr || length == 0 || !vec) return -1;
    
    // All pages are in core (RAM)
    size_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    for(size_t i = 0; i < pages; i++) {
        vec[i] = 1; // Page is in memory
    }
    return 0;
}

int sys_madvise(void *addr, size_t length, int advice) {
    if(!addr || length == 0) return -1;
    // For now, treat all advice as successful
    return 0;
}

int sys_shmget(key_t key, size_t size, int shmflg) {
    // Simplified implementation
    static int shm_id_counter = 1;
    return shm_id_counter++;
}

void *sys_shmat(int shmid, const void *shmaddr, int shmflg) {
    // Simplified implementation - allocate shared memory
    return sys_mmap((void*)shmaddr, 4096, 3, 0x10, -1, 0); // RW, shared
}

int sys_shmctl(int shmid, int cmd, struct shmid_ds *buf) {
    // Simplified implementation
    switch(cmd) {
        case 0: // IPC_RMID
            return 0;
        case 1: // IPC_SET
            return 0;
        case 2: // IPC_STAT
            if(buf) {
                memset(buf, 0, sizeof(struct shmid_ds));
                return 0;
            }
            return -1;
        default:
            return -1;
    }
}

int sys_dup(int oldfd) {
    if(oldfd < 0 || oldfd >= MAX_OPEN_FILES || !current_process->open_files[oldfd]) return -1;
    
    for(int i = 0; i < MAX_OPEN_FILES; i++) {
        if(!current_process->open_files[i]) {
            current_process->open_files[i] = current_process->open_files[oldfd];
            return i;
        }
    }
    return -1;
}

int sys_dup2(int oldfd, int newfd) {
    if(oldfd < 0 || oldfd >= MAX_OPEN_FILES || !current_process->open_files[oldfd]) return -1;
    if(newfd < 0 || newfd >= MAX_OPEN_FILES) return -1;
    
    if(newfd == oldfd) return newfd; // Same file descriptor
    
    if(current_process->open_files[newfd]) {
        sys_close(newfd);
    }
    
    current_process->open_files[newfd] = current_process->open_files[oldfd];
    return newfd;
}

int sys_pause(void) {
    current_process->state = PROC_BLOCKED;
    schedule();
    return 0; // Should never reach here
}

int sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    if(!req) return -1;
    
    // For simplicity, sleep for specified time
    // In real implementation, would use timer and block process
    return 0;
}

int sys_getitimer(int which, struct itimerval *curr_value) {
    if(!curr_value) return -1;
    
    // Initialize to zero for all timers
    memset(curr_value, 0, sizeof(struct itimerval));
    return 0;
}

unsigned int sys_alarm(unsigned int seconds) {
    // Simplified implementation
    return 0;
}

int sys_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    if(old_value) {
        memset(old_value, 0, sizeof(struct itimerval));
    }
    return 0;
}

pid_t sys_getpid(void) {
    return current_process->id;
}

ssize_t sys_sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
    if(out_fd < 0 || out_fd >= MAX_OPEN_FILES || !current_process->open_files[out_fd]) return -1;
    if(in_fd < 0 || in_fd >= MAX_OPEN_FILES || !current_process->open_files[in_fd]) return -1;
    
    // Simple implementation using read/write
    char buffer[4096];
    ssize_t total_sent = 0;
    
    while(count > 0) {
        size_t to_read = (count < sizeof(buffer)) ? count : sizeof(buffer);
        ssize_t bytes_read = sys_read(in_fd, buffer, to_read);
        if(bytes_read <= 0) break;
        
        ssize_t bytes_written = sys_write(out_fd, buffer, bytes_read);
        if(bytes_written <= 0) break;
        
        total_sent += bytes_written;
        count -= bytes_written;
        
        if(offset) {
            *offset += bytes_written;
        }
        
        if(bytes_written != bytes_read) break; // Partial write
    }
    
    return total_sent;
}

int sys_socket(int domain, int type, int protocol) {
    // Simplified implementation - always fail since no network support yet
    return -1;
}

int sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return -1;
}

int sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return -1;
}

ssize_t sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    return -1;
}

ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return -1;
}

ssize_t sys_sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return -1;
}

ssize_t sys_recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return -1;
}

int sys_shutdown(int sockfd, int how) {
    return -1;
}

int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return -1;
}

int sys_listen(int sockfd, int backlog) {
    return -1;
}

int sys_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return -1;
}

int sys_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return -1;
}

int sys_socketpair(int domain, int type, int protocol, int sv[2]) {
    return -1;
}

int sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    return -1;
}

int sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return -1;
}

int sys_clone(unsigned long flags, void *stack, int *parent_tid, int *child_tid, unsigned long tls) {
    struct process *child = copy_process(current_process);
    if(!child) return -1;
    
    if(stack) {
        child->regs->rsp = (uint64_t)stack;
    }
    
    if(parent_tid) {
        *parent_tid = child->id;
    }
    
    child->state = PROC_READY;
    return child->id;
}

int sys_fork(void) {
    struct process *child = copy_process(current_process);
    if(!child) return -1;
    
    child->state = PROC_READY;
    return child->id;
}

int sys_vfork(void) {
    return sys_fork();
}

int sys_execve(const char *filename, char *const argv[], char *const envp[]) {
    if(!filename) return -1;
    
    void *program_data = get_free_page();
    if(!program_data) return -1;
    
    int fd = sys_open(filename, 0, 0); // O_RDONLY
    if(fd < 0) {
        free_page((paddr_t)program_data);
        return -1;
    }
    
    // Read program into memory
    ssize_t bytes_read = sys_read(fd, program_data, 4096);
    sys_close(fd);
    
    if(bytes_read <= 0) {
        free_page((paddr_t)program_data);
        return -1;
    }
    
    // Load ELF and update process state - simplified
    // For now just reset registers to entry point
    current_process->regs->rip = 0x400000; // Entry point
    current_process->regs->rsp = 0x500000; // Stack pointer
    
    free_page((paddr_t)program_data);
    
    return 0;
}

int sys_exit_group(int status) {
    current_process->state = PROC_ZOMBIE;
    current_process->exit_code = status;
    schedule();
    return 0;
}

int sys_waitid(int idtype, int id, struct siginfo *infop, int options, struct rusage *ru) {
    // Simplified implementation
    if(ru) memset(ru, 0, sizeof(struct rusage));
    return 0;
}

int sys_kill(int pid, int sig) {
    if(pid <= 0) return -1;
    
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if(processes[i].id == pid && processes[i].state != PROC_FREE) {
            if(sig == 9) { // SIGKILL
                processes[i].state = PROC_ZOMBIE;
                processes[i].exit_code = -1;
            } else if(sig == 0) {
                // Just test if process exists
            } else {
                // Deliver signal
                if(!(current_process->blocked_signals & (1UL << sig))) {
                    // Call signal handler if available
                    if(current_process->signal_handlers[sig] && 
                       current_process->signal_handlers[sig] != 1) { // Not SIG_IGN
                        if(current_process->signal_handlers[sig] == 0) { // SIG_DFL
                            if(sig == 13 || sig == 17) { // SIGPIPE or SIGCHLD - ignore
                                // Do nothing
                            } else {
                                // Terminate process for other default signals
                                processes[i].state = PROC_ZOMBIE;
                                processes[i].exit_code = -1;
                            }
                        }
                    }
                }
            }
            return 0;
        }
    }
    return -1;
}

int sys_tkill(int tid, int sig) {
    return sys_kill(tid, sig);
}

int sys_tgkill(int tgid, int tid, int sig) {
    return sys_kill(tid, sig);
}

int sys_sigaltstack(const stack_t *ss, stack_t *oss) {
    if(oss) {
        oss->ss_sp = current_process->signal_stack;
        oss->ss_flags = (current_process->signal_stack_active) ? 2 : 1; // SS_ONSTACK or SS_DISABLE
        oss->ss_size = current_process->signal_stack_size;
    }
    
    if(ss) {
        if(ss->ss_flags & 2) { // SS_DISABLE
            current_process->signal_stack = NULL;
            current_process->signal_stack_active = 0;
        } else {
            current_process->signal_stack = ss->ss_sp;
            current_process->signal_stack_size = ss->ss_size;
            current_process->signal_stack_active = 0;
        }
    }
    
    return 0;
}

int sys_rt_sigsuspend(const sigset_t *mask, size_t sigsetsize) {
    if(!mask) return -1;
    
    sigset_t old_mask = current_process->blocked_signals;
    current_process->blocked_signals = *mask;
    
    current_process->state = PROC_BLOCKED;
    schedule();
    
    current_process->blocked_signals = old_mask;
    return 0;
}

int sys_rt_sigpending(sigset_t *set, size_t sigsetsize) {
    if(!set) return -1;
    
    // No pending signals in this simple implementation
    *set = 0;
    return 0;
}

int sys_rt_sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout, size_t sigsetsize) {
    if(!set) return -1;
    
    // No signals to wait for in this simple implementation
    return -1;
}

int sys_rt_sigqueueinfo(int pid, int sig, const siginfo_t *info) {
    return sys_kill(pid, sig);
}

int sys_sigwaitinfo(const sigset_t *set, siginfo_t *info) {
    if(!set) return -1;
    
    // No signals in this simple implementation
    return -1;
}

int sys_chown(const char *pathname, uid_t owner, gid_t group) {
    if(!pathname) return -1;
    return 0;
}

int sys_fchown(int fd, uid_t owner, gid_t group) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !current_process->open_files[fd]) return -1;
    return 0;
}

int sys_lchown(const char *pathname, uid_t owner, gid_t group) {
    if(!pathname) return -1;
    return 0;
}

mode_t sys_umask(mode_t mask) {
    mode_t old_mask = current_process->umask;
    current_process->umask = mask;
    return old_mask;
}

int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
    if(tv) {
        tv->tv_sec = 0; // Placeholder - should get real time
        tv->tv_usec = 0;
    }
    if(tz) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }
    return 0;
}

int sys_getrlimit(int resource, struct rlimit *rlim) {
    if(!rlim) return -1;
    
    switch(resource) {
        case 0: // RLIMIT_AS
        case 2: // RLIMIT_DATA
            rlim->rlim_cur = 1024 * 1024 * 1024; // 1GB
            rlim->rlim_max = 1024 * 1024 * 1024; // 1GB
            break;
        case 3: // RLIMIT_STACK
            rlim->rlim_cur = 8 * 1024 * 1024; // 8MB
            rlim->rlim_max = 8 * 1024 * 1024; // 8MB
            break;
        default:
            rlim->rlim_cur = ~0UL; // RLIM_INFINITY
            rlim->rlim_max = ~0UL; // RLIM_INFINITY
            break;
    }
    return 0;
}

int sys_getrusage(int who, struct rusage *usage) {
    if(!usage) return -1;
    memset(usage, 0, sizeof(struct rusage));
    return 0;
}

int sys_sysinfo(struct sysinfo *info) {
    if(!info) return -1;
    memset(info, 0, sizeof(struct sysinfo));
    return 0;
}

clock_t sys_times(struct tms *buf) {
    if(buf) {
        buf->tms_utime = 0;
        buf->tms_stime = 0;
        buf->tms_cutime = 0;
        buf->tms_cstime = 0;
    }
    return 0;
}

long sys_ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data) {
    return -1;
}

uid_t sys_getuid(void) {
    return 0; // Root user
}

int sys_syslog(int type, char *bufp, int len) {
    return -1;
}

gid_t sys_getgid(void) {
    return 0; // Root group
}

int sys_setuid(uid_t uid) {
    return 0;
}

int sys_setgid(gid_t gid) {
    return 0;
}

uid_t sys_geteuid(void) {
    return 0; // Effective UID
}

gid_t sys_getegid(void) {
    return 0; // Effective GID
}

int sys_setpgid(pid_t pid, pid_t pgid) {
    return 0;
}

pid_t sys_getppid(void) {
    return current_process->parent_id;
}

pid_t sys_getpgrp(void) {
    return 0;
}

pid_t sys_setsid(void) {
    return 0;
}

int sys_setreuid(uid_t ruid, uid_t euid) {
    return 0;
}

int sys_setregid(gid_t rgid, gid_t egid) {
    return 0;
}

int sys_getgroups(int size, gid_t list[]) {
    if(size > 0 && list) {
        list[0] = 0; // Root group
        return 1;
    }
    return 0;
}

int sys_setgroups(int size, const gid_t *list) {
    return 0;
}

int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid) {
    return 0;
}

int sys_getresuid(uid_t *ruid, uid_t *euid, uid_t *suid) {
    if(ruid) *ruid = 0;
    if(euid) *euid = 0;
    if(suid) *suid = 0;
    return 0;
}

int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
    return 0;
}

int sys_getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid) {
    if(rgid) *rgid = 0;
    if(egid) *egid = 0;
    if(sgid) *sgid = 0;
    return 0;
}

pid_t sys_getpgid(pid_t pid) {
    return 0;
}

int sys_setfsuid(uid_t fsuid) {
    return 0;
}

int sys_setfsgid(gid_t fsgid) {
    return 0;
}

pid_t sys_getsid(pid_t pid) {
    return 0;
}

int sys_capget(cap_user_header_t header, const cap_user_data_t data) {
    if(!header) return -1;
    return 0;
}

int sys_capset(cap_user_header_t header, const cap_user_data_t data) {
    return -1;
}

int sys_rt_sigreturn(void) {
    return 0;
}

int sys_setpriority(int which, int who, int prio) {
    return 0;
}

int sys_getpriority(int which, int who) {
    return 0;
}

int sys_sched_setparam(pid_t pid, const struct sched_param *param) {
    return 0;
}

int sys_sched_getparam(pid_t pid, struct sched_param *param) {
    if(param) param->sched_priority = 0;
    return 0;
}

int sys_sched_setscheduler(pid_t pid, int policy, const struct sched_param *param) {
    return 0;
}

int sys_sched_getscheduler(pid_t pid) {
    return 0; // SCHED_OTHER
}

int sys_sched_get_priority_max(int policy) {
    return 99;
}

int sys_sched_get_priority_min(int policy) {
    return 0;
}

int sys_sched_rr_get_interval(pid_t pid, struct timespec *interval) {
    if(interval) {
        interval->tv_sec = 0;
        interval->tv_nsec = 100000000; // 100ms
    }
    return 0;
}

int sys_mlock(const void *addr, size_t len) {
    return 0;
}

int sys_munlock(const void *addr, size_t len) {
    return 0;
}

int sys_mlockall(int flags) {
    return 0;
}

int sys_munlockall(void) {
    return 0;
}

int sys_vhangup(void) {
    return 0;
}

int sys_modify_ldt(int func, void *ptr, unsigned long bytecount) {
    return -1;
}

int sys_pivot_root(const char *new_root, const char *put_old) {
    return -1;
}

int sys__sysctl(struct __sysctl_args *args) {
    return -1;
}

int sys_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    return 0;
}

int sys_arch_prctl(int code, unsigned long addr) {
    return 0;
}

int sys_adjtimex(struct timex *buf) {
    return -1;
}

int sys_setrlimit(int resource, const struct rlimit *rlim) {
    return 0;
}

int sys_chroot(const char *path) {
    return -1;
}

int sys_sync(void) {
    return 0;
}

int sys_acct(const char *filename) {
    return -1;
}

int sys_settimeofday(const struct timeval *tv, const struct timezone *tz) {
    return -1;
}

int sys_mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) {
    return -1;
}

int sys_umount2(const char *target, int flags) {
    return -1;
}

int sys_swapon(const char *path, int swapflags) {
    return -1;
}

int sys_swapoff(const char *path) {
    return -1;
}

int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg) {
    return -1;
}

int sys_sethostname(const char *name, size_t len) {
    return -1;
}

int sys_setdomainname(const char *name, size_t len) {
    return -1;
}

int sys_iopl(int level) {
    return -1;
}

int sys_ioperm(unsigned long from, unsigned long num, int turn_on) {
    return -1;
}

int sys_create_module(const char *name, size_t size) {
    return -1;
}

int sys_init_module(void *module_image, unsigned long len, const char *param_values) {
    return -1;
}

int sys_delete_module(const char *name, int flags) {
    return -1;
}

int sys_get_kernel_syms(struct kernel_sym *table) {
    return -1;
}

int sys_query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret) {
    return -1;
}

int sys_quotactl(int cmd, const char *special, int id, caddr_t addr) {
    return -1;
}

int sys_nfsservctl(int cmd, struct nfsctl_arg *arg, union nfsd_fsctl *fsc) {
    return -1;
}

int sys_getpmsg(void *ctlptr, void *dataptr, int *bandp, int *flagsp, int *controllen, int *datalen) {
    return -1;
}

int sys_putpmsg(void *ctlptr, void *dataptr, int band, int flags, int controllen, int datalen) {
    return -1;
}

int sys_afs_syscall(void *template, int key, int arg, void *val, int vallen, int op) {
    return -1;
}

int sys_tuxcall(int cmd, void *u_data, int len) {
    return -1;
}

int sys_security(int call, void *ptr, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5) {
    return -1;
}

pid_t sys_gettid(void) {
    return current_process->id;
}

int sys_readahead(int fd, loff_t offset, size_t count) {
    return 0;
}

int sys_setxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
    return -1;
}

int sys_lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags) {
    return -1;
}

int sys_fsetxattr(int fd, const char *name, const void *value, size_t size, int flags) {
    return -1;
}

int sys_getxattr(const char *path, const char *name, void *value, size_t size) {
    return -1;
}

int sys_lgetxattr(const char *path, const char *name, void *value, size_t size) {
    return -1;
}

int sys_fgetxattr(int fd, const char *name, void *value, size_t size) {
    return -1;
}

int sys_listxattr(const char *path, char *list, size_t size) {
    return -1;
}

int sys_llistxattr(const char *path, char *list, size_t size) {
    return -1;
}

int sys_flistxattr(int fd, char *list, size_t size) {
    return -1;
}

int sys_removexattr(const char *path, const char *name) {
    return -1;
}

int sys_lremovexattr(const char *path, const char *name) {
    return -1;
}

int sys_fremovexattr(int fd, const char *name) {
    return -1;
}

time_t sys_time(time_t *tloc) {
    time_t t = 0; // Placeholder
    if(tloc) *tloc = t;
    return t;
}

int sys_futex(int *uaddr, int futex_op, int val, const struct timespec *timeout, int *uaddr2, int val3) {
    return -1;
}

int sys_sched_setaffinity(pid_t pid, size_t cpusetsize, const unsigned long *mask) {
    return 0;
}

int sys_sched_getaffinity(pid_t pid, size_t cpusetsize, unsigned long *mask) {
    if(mask) *mask = 1; // CPU 0
    return 0;
}

int sys_set_thread_area(struct user_desc *u_info) {
    return 0;
}

int sys_io_setup(unsigned nr_events, struct aio_context_t *ctx_idp) {
    return -1;
}

int sys_io_destroy(aio_context_t ctx_id) {
    return -1;
}

int sys_io_getevents(aio_context_t ctx_id, long min_nr, long nr, struct io_event *events, struct timespec *timeout) {
    return -1;
}

int sys_io_submit(aio_context_t ctx_id, long nr, struct iocb **iocbpp) {
    return -1;
}

int sys_io_cancel(aio_context_t ctx_id, struct iocb *iocb, struct io_event *result) {
    return -1;
}

int sys_get_thread_area(struct user_desc *u_info) {
    return 0;
}

int sys_lookup_dcookie(uint64_t cookie64, char *buf, size_t len) {
    return -1;
}

int sys_epoll_create(int size) {
    return -1;
}

int sys_epoll_ctl_old(int epid, int op, int fd, struct epoll_event *event) {
    return -1;
}

int sys_epoll_wait_old(int epid, struct epoll_event *events, int maxevents, int timeout) {
    return -1;
}

int sys_remap_file_pages(void *addr, size_t size, int prot, size_t pgoff, int flags) {
    return 0;
}

int sys_getdents64(int fd, struct dirent *dirp, size_t count) {
    return -1;
}

int sys_set_tid_address(int *tidptr) {
    return current_process->id;
}

int sys_restart_syscall(void) {
    return -1;
}

int sys_semtimedop(int semid, struct sembuf *sops, unsigned nsops, const struct timespec *timeout) {
    return -1;
}

int sys_fadvise64(int fd, loff_t offset, size_t len, int advice) {
    return 0;
}

int sys_timer_create(clockid_t clockid, struct sigevent *sevp, timer_t *timerid) {
    return -1;
}

int sys_timer_settime(timer_t timerid, int flags, const struct itimerspec *new_value, struct itimerspec *old_value) {
    return -1;
}

int sys_timer_gettime(timer_t timerid, struct itimerspec *curr_value) {
    return -1;
}

int sys_timer_getoverrun(timer_t timerid) {
    return 0;
}

int sys_timer_delete(timer_t timerid) {
    return -1;
}

int sys_clock_settime(clockid_t clk_id, const struct timespec *tp) {
    return -1;
}

int sys_clock_gettime(clockid_t clk_id, struct timespec *tp) {
    if(tp) {
        tp->tv_sec = 0;
        tp->tv_nsec = 0;
    }
    return 0;
}

int sys_clock_getres(clockid_t clk_id, struct timespec *res) {
    if(res) {
        res->tv_sec = 0;
        res->tv_nsec = 1; // 1ns resolution
    }
    return 0;
}

int sys_clock_nanosleep(clockid_t clockid, int flags, const struct timespec *request, struct timespec *remain) {
    return -1;
}

int sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    return -1;
}

int sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    return -1;
}

int sys_utimes(const char *filename, const struct timeval *times) {
    return -1;
}

int sys_vserver(int unique, int call, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5) {
    return -1;
}

int sys_mbind(void *addr, unsigned long len, int mode, const unsigned long *nmask, unsigned long maxnode, unsigned flags) {
    return 0;
}

int sys_set_mempolicy(int mode, const unsigned long *nmask, unsigned long maxnode) {
    return 0;
}

int sys_get_mempolicy(int *mode, unsigned long *nmask, unsigned long maxnode, void *addr, unsigned flags) {
    return 0;
}

int sys_mq_open(const char *name, int oflag, mode_t mode, struct mq_attr *attr) {
    return -1;
}

int sys_mq_unlink(const char *name) {
    return -1;
}

int sys_mq_timedsend(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned msg_prio, const struct timespec *abs_timeout) {
    return -1;
}

int sys_mq_timedreceive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned *msg_prio, const struct timespec *abs_timeout) {
    return -1;
}

int sys_mq_notify(mqd_t mqdes, const struct sigevent *notification) {
    return -1;
}

int sys_mq_getsetattr(mqd_t mqdes, const struct mq_attr *newattr, struct mq_attr *oldattr) {
    return -1;
}

int sys_kexec_load(unsigned long entry, unsigned long nr_segments, struct kexec_segment *segments, unsigned long flags) {
    return -1;
}

int sys_add_key(const char *type, const char *description, const void *payload, size_t plen, key_serial_t keyring) {
    return -1;
}

int sys_request_key(const char *type, const char *description, const char *callout_info, key_serial_t destringid) {
    return -1;
}

int sys_keyctl(int cmd, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    return -1;
}

int sys_ioprio_set(int which, int who, int ioprio) {
    return 0;
}

int sys_ioprio_get(int which, int who) {
    return 0;
}

int sys_inotify_init(void) {
    return -1;
}

int sys_inotify_add_watch(int fd, const char *pathname, uint32_t mask) {
    return -1;
}

int sys_inotify_rm_watch(int fd, int wd) {
    return -1;
}

int sys_migrate_pages(pid_t pid, unsigned long maxnode, const unsigned long *from, const unsigned long *to) {
    return -1;
}

int sys_openat(int dirfd, const char *pathname, int flags, ...) {
    va_list args;
    va_start(args, flags);
    mode_t mode = va_arg(args, mode_t);
    va_end(args);
    return sys_open(pathname, flags, mode);
}

int sys_mkdirat(int dirfd, const char *pathname, mode_t mode) {
    return -1;
}

int sys_mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev) {
    return -1;
}

int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
    return -1;
}

int sys_futimesat(int dirfd, const char *pathname, const struct timeval times[2]) {
    return -1;
}

int sys_newfstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags) {
    return sys_stat(pathname, statbuf);
}

int sys_unlinkat(int dirfd, const char *pathname, int flags) {
    return -1;
}

int sys_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
    return -1;
}

int sys_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) {
    return -1;
}

int sys_symlinkat(const char *target, int newdirfd, const char *linkpath) {
    return -1;
}

int sys_readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz) {
    return -1;
}

int sys_fchmodat(int dirfd, const char *pathname, mode_t mode, int flags) {
    return -1;
}

int sys_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    return sys_access(pathname, mode);
}

int sys_pselect6(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timespec *timeout, void *sigmask) {
    return sys_select(nfds, readfds, writefds, exceptfds, (struct timeval*)timeout);
}

int sys_ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts, const sigset_t *sigmask, size_t sigsetsize) {
    return sys_poll(fds, nfds, timeout_ts ? (timeout_ts->tv_sec * 1000 + timeout_ts->tv_nsec / 1000000) : -1);
}

int sys_unshare(int flags) {
    return -1;
}

int sys_set_robust_list(struct robust_list_head *head, size_t len) {
    return 0;
}

int sys_get_robust_list(int pid, struct robust_list_head **head_ptr, size_t *len_ptr) {
    return -1;
}

int sys_splice(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags) {
    return -1;
}

int sys_tee(int fd_in, int fd_out, size_t len, unsigned int flags) {
    return -1;
}

int sys_sync_file_range(int fd, off_t offset, off_t nbytes, unsigned int flags) {
    return 0;
}

int sys_vmsplice(int fd, const struct iovec *iov, unsigned long nr_segs, unsigned int flags) {
    return -1;
}

int sys_move_pages(pid_t pid, unsigned long count, void **pages, const int *nodes, int *status, int flags) {
    return -1;
}

int sys_utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags) {
    return -1;
}

int sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask, size_t sigsetsize) {
    return sys_epoll_wait(epfd, events, maxevents, timeout);
}

int sys_signalfd(int fd, const sigset_t *mask, size_t sizemask, int flags) {
    return -1;
}

int sys_timerfd_create(int clockid, int flags) {
    return -1;
}

int sys_eventfd(unsigned int initval, int flags) {
    return -1;
}

int sys_fallocate(int fd, int mode, off_t offset, off_t len) {
    return -1;
}

int sys_timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value) {
    return -1;
}

int sys_timerfd_gettime(int fd, struct itimerspec *curr_value) {
    if(curr_value) {
        curr_value->it_interval.tv_sec = 0;
        curr_value->it_interval.tv_nsec = 0;
        curr_value->it_value.tv_sec = 0;
        curr_value->it_value.tv_nsec = 0;
    }
    return 0;
}

// Network system call implementations
int net_send(void* packet, size_t length) {
    i8254x_transmit(packet, (uint32_t)length);
    return length;
}

int net_receive(void* packet, size_t max_length) {
    uint32_t length = i8254x_poll(packet);
    return (int)length;
}
