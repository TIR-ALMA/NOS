#include "printf.h"
#include "string.h"
#include "drivers/vga.h"

static void print_int(long num) {
    if(num < 0) {
        vga_write_string("-");
        num = -num;
    }
    
    if(num == 0) {
        vga_write_string("0");
        return;
    }
    
    char buf[32];
    int i = 0;
    while(num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    for(int j = i-1; j >= 0; j--) {
        char tmp[2] = {buf[j], 0};
        vga_write_string(tmp);
    }
}

void printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    const char *p = format;
    while(*p) {
        if(*p == '%') {
            p++;
            switch(*p) {
                case 'd':
                case 'i':
                    print_int(va_arg(args, int));
                    break;
                case 'x':
                    vga_write_string("0x"); // Просто заглушка
                    break;
                case 's':
                    vga_write_string(va_arg(args, char*));
                    break;
                case '%':
                    vga_write_string("%");
                    break;
                default:
                    vga_write_string("%");
                    break;
            }
        } else {
            char c[2] = {*p, 0};
            vga_write_string(c);
        }
        p++;
    }
    
    va_end(args);
}
