#ifndef TIMER_H
#define TIMER_H

void init_timer();

void tidy_timer();

void clear_sys();

void write_tac(uint8_t value);

void write_tima(uint8_t value);

uint32_t system_clock_pulse();

char *get_emu_time(char *buffer, size_t size);

#endif