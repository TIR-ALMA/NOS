// drivers/vga.c
#include "vga.h"
#include "lib/string.h"

// Аппаратный буфер VGA
static uint16_t* vga_buffer = (uint16_t*)0xB8000;
static uint16_t cursor_x = 0;
static uint16_t cursor_y = 0;
static uint8_t color = 0x07; // Белый на черном по умолчанию

void vga_init() {
    // Сброс курсора
    cursor_x = 0;
    cursor_y = 0;
    // Очистка экрана
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (' ' | (color << 8));
    }
    vga_update_cursor();
}

static void vga_scroll() {
    // Сдвиг всех строк вверх на одну
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
    }
    // Очистка последней строки
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga_buffer[i] = (' ' | (color << 8));
    }
}

static void vga_put_entry_at(char c, uint8_t col, uint16_t x, uint16_t y) {
    const uint16_t index = y * VGA_WIDTH + x;
    vga_buffer[index] = c | (col << 8);
}

static void vga_advance_cursor() {
    cursor_x++;
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = VGA_HEIGHT - 1;
        vga_scroll();
    }
}

void vga_write_char(char c) {
    switch (c) {
        case '\n':
            cursor_x = 0;
            cursor_y++;
            break;
        case '\b': // Backspace
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = VGA_WIDTH - 1;
            }
            vga_put_entry_at(' ', color, cursor_x, cursor_y);
            break;
        case '\t':
            // Просто добавляем пробелы до следующей границы табуляции (каждые 8)
            for (int i = 0; i < 8 - (cursor_x % 8); i++) {
                vga_put_entry_at(' ', color, cursor_x, cursor_y);
                vga_advance_cursor();
            }
            break;
        default:
            vga_put_entry_at(c, color, cursor_x, cursor_y);
            vga_advance_cursor();
            break;
    }

    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = VGA_HEIGHT - 1;
        vga_scroll();
    }
}

void vga_write_string(const char *str) {
    while (*str) {
        vga_write_char(*str);
        str++;
    }
    vga_update_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = ((bg & 0x0F) << 4) | (fg & 0x0F);
}

void vga_update_cursor() {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    // Отправка позиции курсора в порты VGA
    __asm__ volatile ("outb %0, %1" :: "a"(0x0F), "Nd"(0x3D4));
    __asm__ volatile ("outb %0, %1" :: "a"((uint8_t)(pos & 0xFF)), "Nd"(0x3D5));
    __asm__ volatile ("outb %0, %1" :: "a"(0x0E), "Nd"(0x3D4));
    __asm__ volatile ("outb %0, %1" :: "a"((uint8_t)((pos >> 8) & 0xFF)), "Nd"(0x3D5));
}

void vga_clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (' ' | (color << 8));
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

