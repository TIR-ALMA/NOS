// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

void keyboard_init();
void keyboard_handler();

// --- Новые объявления ---
extern volatile int keyboard_buffer_count; // Для отладки или внешнего доступа
char keyboard_get_char();
int keyboard_chars_available();
// --- Конец новых объявлений ---

#endif

