#include <stdlib.h>
#include "emulator.h"
#include "cartridge.h"
#include "cpu.h"
#include "clock.h"

static EmulatorState *emu_state;

void start_emulator()
{
    set_cpu_running(true);
    emu_state->running = true;
    while(emu_state->running)
    {
       emu_state->running = machine_cycle();
    }
}

void tidy_emulator()
{
    tidy_clock();
    tidy_cpu();
    tidy_cartridge();
    free(emu_state);
    emu_state = NULL;
}

void init_emulator(char *file_path, bool testing)
{
    emu_state = (EmulatorState*) malloc(sizeof(EmulatorState));
    init_clock();
    set_freq(100);
    init_cartridge(file_path, testing);
    init_cpu(testing);
}