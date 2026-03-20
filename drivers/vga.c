#include "vga.h"
#include "lib/string.h"

void vga_init() {
    // Инициализация VGA - в простом случае ничего делать не нужно
}

void vga_write_string(const char *str) {
    // Простая реализация - вывод в VGA буфер
    static char *video = (char*)0xB8000;
    static int cursor = 0;
    
    while(*str) {
        video[cursor] = *str;
        video[cursor+1] = 0x07; // Белый на черном
        cursor += 2;
        str++;
    }
}
