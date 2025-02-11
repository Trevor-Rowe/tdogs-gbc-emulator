#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "cpu.h"

typedef struct
{
    double freq;
    uint8_t sec_delay;
    long nano_delay;
    uint8_t cycle_count;

} ClockState;

static ClockState *settings;

static void normalize_timespec(struct timespec *ts) {
    // Normalize tv_nsec if it's out of bounds
    while (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
    // Ensure tv_nsec is not negative (shouldn't happen in normal cases)
    if (ts->tv_nsec < 0) {
        ts->tv_nsec = 0;
    }
}

static void pulse_system_clock()
{ // BASE CLOCK
    struct timespec ts;
    ts.tv_sec  = settings->sec_delay;
    ts.tv_nsec = settings->nano_delay;
    normalize_timespec(&ts);
    //if(get_cpu_info()->testing) printf(" %d ", settings->cycle_count);
    settings->cycle_count += 1;
    // TODO: Peripheral Ping -> Ping them and return possible interrupts :)
    nanosleep(&ts, NULL);
}

static bool system_cycles()
{ // WRAPPER CLOCK
   int ticks = MACHINE_TO_SYS_RATIO;
   while (ticks > 0)
   {
        pulse_system_clock();
        ticks -= 1; 
    }
}

void pulse_clock()
{
    system_cycles();
}

// Must set seconds first.
void calculate_nano_delay()
{
    double period = (1.0 / settings->freq);
    long nanoseconds = (long) ((period - settings->sec_delay) * 1.0E9);
    settings->nano_delay = nanoseconds;
}

void calculate_sec_delay()
{
    uint8_t seconds = (uint8_t) (1.0 / settings->freq);
    settings->sec_delay = seconds;
}

void set_freq(double new_freq)
{
    settings->freq = new_freq;
    calculate_sec_delay();
    calculate_nano_delay();
    printf("%d seconds\n", settings->sec_delay);
    printf("%ld nanoseconds\n\n", settings->nano_delay);
}

double get_freq()
{
    return settings->freq;
}

void init_clock()
{
    settings = (ClockState*) malloc(sizeof(ClockState));
    set_freq(BASE_CLOCK_SPEED);
}

void tidy_clock()
{
    free(settings);
    settings = NULL;
}

uint8_t get_cycles()
{
    return settings->cycle_count;
}

void reset_cycles()
{
    settings->cycle_count = 0;
}