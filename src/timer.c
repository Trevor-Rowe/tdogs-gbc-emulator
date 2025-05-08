#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "common.h"
#include "ppu.h"
#include "cpu.h"
#include "mmu.h"
#include "timer.h"
#include "logger.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)
#define DEFAULT_MACHINE_CYCLE_DELAY   4 // System  Cycles
#define DEFAULT_TIMA_OVERFLOW_DELAY   4 // Machine Cycles (1 Cycle After Enabled)

typedef void (*EventHandler)();

typedef struct // Event handler will interpret delay in T-Cycles.
{
    bool          active;
    int8_t         delay;
    EventHandler handler;

} SystemCycleEvent;

static const uint8_t sys_shift_table[4] = {
    9, // TAC = 0b00 → 4096 Hz
    3, // TAC = 0b01 → 262144 Hz
    5, // TAC = 0b10 → 65536 Hz
    7  // TAC = 0b11 → 16384 Hz
};

static SystemCycleEvent*  tima_overflow;
static uint32_t             current_dot;

static uint16_t      sys;
static uint8_t     *div_;
static uint8_t      *tac;
static uint8_t      *tma;
static uint8_t     *tima;

bool prev_sys_bit;

static bool get_current_sys_bit()
{
    uint8_t    select = (*tac) & LOWER_2_MASK;
    bool curr_sys_bit = (sys >> sys_shift_table[select]) & BIT_0_MASK;
    return curr_sys_bit;
}

static void inc_tima(bool incrementing)
{
    (*tima) += 1;

    if ((*tima == 0) && incrementing) // Overflow only triggers on SYS increments.
    { // overflow
        tima_overflow->active = true;
        tima_overflow-> delay = DEFAULT_TIMA_OVERFLOW_DELAY;
    }
}

static void check_tima_inc(bool incrementing) // Falling edge detection multiplexer. 
{
    bool   inc_enable = (*tac) & BIT_2_MASK;
    bool curr_sys_bit = get_current_sys_bit();
    if (prev_sys_bit && !curr_sys_bit && inc_enable) inc_tima(incrementing);
    prev_sys_bit = curr_sys_bit;
}

static void write_sys(uint16_t value, bool incrementing)
{
    sys   =  value & LOWER_14_MASK;
    *div_ = (sys >> 6) & LOWER_BYTE_MASK;
    check_tima_inc(incrementing);
}

void clear_sys() // MMU Interface for writing to DIV. 
{ // 'Writing to DIV'
    write_sys(0, false);
}

void write_tima(uint8_t value) // MMU Interface for writing to TIMA.
{
    (*tima) = value;
    prev_sys_bit = get_current_sys_bit(); // Resync to prevent increment pulse.
}   

void write_tac(uint8_t value)
{
    (*tac) = value;
    check_tima_inc(false);
}

static void check_cycle_event(SystemCycleEvent *event)
{
    if (event->active)
    {
        if (--event->delay <= 0)
        {
            event->handler();
        }
    }
}

static void tima_overflow_handler()
{
    (*tima) = (*tma);
    request_interrupt(TIMER_INTERRUPT_CODE);
    tima_overflow->active = false; // Consume the Event.
}

uint32_t system_clock_pulse() // Emulator Interface
{
    dot(current_dot);            // PPU
    check_dma();                 // MMU

    if ((sys % get_machine_cycle_scaler()) == 0)
    {
        machine_cycle();
    }
    check_cycle_event(tima_overflow);
    write_sys((sys + 1), true);  // TIMER

    current_dot = ((current_dot + 1) % DOT_PER_FRAME);
    return current_dot;
}

char *get_emu_time(char *buffer, size_t size)
{
    snprintf(
        buffer, 
        size, 
        "%02X:%02X:%02X:%04X", 
        (*tac), (*tma), (*tima), sys
    );
    return buffer;
}

void init_timer()
{
    tima_overflow           = (SystemCycleEvent*) malloc(sizeof(SystemCycleEvent));
    tima_overflow->  active = false;
    tima_overflow->   delay = DEFAULT_TIMA_OVERFLOW_DELAY;
    tima_overflow-> handler = tima_overflow_handler;

    current_dot  = 0;
    prev_sys_bit = 0;

    sys  = 0; // Grab pointers here carefully.
    div_ = get_memory_pointer(DIV);
    tac  = get_memory_pointer(TAC);
    tma  = get_memory_pointer(TMA);
    tima = get_memory_pointer(TIMA); 
}

void tidy_timer()
{
    free(tima_overflow); tima_overflow = NULL;
}