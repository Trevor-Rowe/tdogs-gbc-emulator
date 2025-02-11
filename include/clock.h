#ifndef CLOCK_H
#define CLOCK_H

double get_freq();

void pulse_clock();

void set_freq(double new_freq);

void init_clock();

void tidy_clock();

uint8_t get_cycles();

void reset_cycles();

#endif