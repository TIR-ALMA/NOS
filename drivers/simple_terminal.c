// simple_terminal.c
#include "lib/printf.h"
#include "drivers/keyboard.h"
#include "lib/string.h" // Для strcpy, strcmp
#include "sched.h"      // Для ps-подобной команды (опционально)
#include "mm.h"         // Для mem-подобной команды (опционально)
#include "types.h"      // Для типов, если нужно

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static int input_pos = 0;

// Функция для получения символа (временно, пока не реализован буфер в keyboard.c)
// Мы будем вызывать её из main_loop и использовать флаг для проверки наличия символа
// Это не идеальный способ, но самый простой с текущими файлами.
static volatile uint8_t last_scancode = 0;
static volatile uint8_t new_char_ready = 0;

// Это наша версия keyboard_handler, которую мы будем вызывать напрямую
// или подключим как настоящий обработчик, если измените keyboard.c
void simple_kb_handler() {
    uint8_t scancode = inb(0x60); // Прямой доступ к порту, как в keyboard.c
    
    if (!(scancode & 0x80)) { // Клавиша нажата
        // Простая карта сканкодов (только для букв, цифр, пробела, Enter, Backspace)
        // Можно расширить по необходимости
        static const char sc_map[] = {
             0,  0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
            'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
            'd', 'f', 'g', 'h', 'j', 'k', 'l', ';','\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
            'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
        };
        if (scancode < sizeof(sc_map)) {
            char c = sc_map[scancode];
            if (c != 0) {
                last_scancode = c; // Сохраняем символ
                new_char_ready = 1; // Устанавливаем флаг
            }
        }
    }
}

// Простая функция для получения символа из "буфера" (на самом деле это просто флаг)
char simple_get_char() {
    if (new_char_ready) {
        new_char_ready = 0;
        return last_scancode;
    }
    return 0; // Нет нового символа
}

void simple_terminal_init() {
    printf("\n=== Простой Терминал Запущен ===\n");
    printf("Введите 'help' для списка команд.\n");
    printf("Введите команду: ");
}

void simple_print_prompt() {
    printf("$ "); // Простое приглашение
}

void simple_execute_command(const char* cmd) {
    // Парсим первую часть команды (без аргументов для простоты)
    char command[INPUT_BUFFER_SIZE];
    int i = 0;
    while (cmd[i] != ' ' && cmd[i] != '\0' && i < INPUT_BUFFER_SIZE - 1) {
        command[i] = cmd[i];
        i++;
    }
    command[i] = '\0';

    if (strcmp(command, "help") == 0) {
        printf("Доступные команды: help, clear, ps, mem, date, echo, exit\n");
    } else if (strcmp(command, "clear") == 0) {
        // printf сам по себе не может очистить экран, просто выводим много пустых строк
        for(int j = 0; j < 30; j++) printf("\n");
        printf("Экран \"очищен\" (выведено 30 пустых строк)\n");
    } else if (strcmp(command, "ps") == 0) {
        printf("PID\tNAME\t\tSTATE\n");
        for(int j = 0; j < MAX_PROCESSES; j++) {
            if(processes[j].state != PROC_FREE) {
                const char* state_str = 
                    processes[j].state == PROC_READY ? "READY" :
                    processes[j].state == PROC_RUNNING ? "RUNNING" :
                    processes[j].state == PROC_BLOCKED ? "BLOCKED" :
                    processes[j].state == PROC_ZOMBIE ? "ZOMBIE" : "UNKNOWN";
                printf("%d\t%s\t\t%s\n", processes[j].id, processes[j].name, state_str);
            }
        }
    } else if (strcmp(command, "mem") == 0) {
        // Просто сообщение, так как точная статистика не реализована в mm.c
        printf("Информация о памяти недоступна в этой версии.\n");
    } else if (strcmp(command, "date") == 0) {
        printf("Текущая дата и время: Пятница, 20 марта 2026 г., 00:00:00\n");
    } else if (strcmp(command, "echo") == 0) {
        // Пропускаем "echo "
        const char* message = cmd + 5;
        if(strlen(message) > 0) {
            printf("%s\n", message);
        } else {
            printf("\n");
        }
    } else if (strcmp(command, "exit") == 0) {
        printf("Выход из терминала. До свидания!\n");
        while(1); // Зацикливание, как в старом коде
    } else {
        printf("Команда '%s' не найдена. Введите 'help'.\n", command);
    }
}

void simple_terminal_run() {
    simple_terminal_init();
    simple_print_prompt();

    while(1) {
        char c = simple_get_char();

        if (c != 0) { // Новый символ получен
            if (c == '\n' || c == '\r') {
                input_buffer[input_pos] = '\0'; // Завершаем строку

                if (input_pos > 0) { // Проверяем, что строка не пустая
                    printf("\n"); // Переходим на новую строку перед выводом результата
                    simple_execute_command(input_buffer);
                }

                // Сбрасываем буфер и приглашение
                input_pos = 0;
                simple_print_prompt();
            } else if (c == '\b') { // Backspace
                if (input_pos > 0) {
                    // Визуально стираем символ (печатаем пробел и возвращаем курсор)
                    printf("\b \b");
                    input_pos--;
                }
            } else if (c >= ' ' && input_pos < INPUT_BUFFER_SIZE - 1) { // Печатаемый символ
                input_buffer[input_pos] = c;
                input_pos++;
                printf("%c", c); // Выводим символ на экран
            }
            // Обновляем флаг после обработки
            new_char_ready = 0;
        }

        // Краткая задержка или другие задачи могут быть здесь
        __asm__ volatile ("pause" ::: "memory");
    }
}

