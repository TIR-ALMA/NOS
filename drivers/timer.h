#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned long timer_ticks;

void timer_init();
void timer_handler();

#endif

