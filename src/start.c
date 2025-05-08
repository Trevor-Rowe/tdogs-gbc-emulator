#include <SDL2/SDL.h>
#include "emulator.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

static void load_rom(char *path)
{
    init_emulator(path, 0x0000);
    start_emulator();
    tidy_emulator();
    SDL_Delay(100);
}

static void run_cpu_test(uint8_t test)
{
    switch(test)
    {
        case 1:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/01-special.gb");
            break;

        case 2:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/02-interrupts.gb");
            break;

        case 3:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/03-op sp,hl.gb");
            break;

        case 4:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/04-op r,imm.gb");
            break;

        case 5:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/05-op rp.gb");
            break;

        case 6:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/06-ld r,r.gb");
            break;

        case 7:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb");
            break;

        case 8:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/08-misc instrs.gb");
            break;

        case 9:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/09-op r,r.gb");
            break;

        case 10:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/10-bit ops.gb");
            break;

        case 11:
            load_rom("../roms/gb-test-roms-master/cpu_instrs/individual/11-op a,(hl).gb");
            break;
    }
}

int main() 
{   
    //for (int i = 1; i <= 11; i++) { run_cpu_test(i); }
    //load_rom("../roms/gb-test-roms-master/instr_timing/instr_timing.gb");
    //load_rom("../roms/gb-test-roms-master/mem_timing/mem_timing.gb");
    //load_rom("../roms/gb-test-roms-master/interrupt_time/interrupt_time.gb");
    //load_rom("../roms/gb-test-roms-master/halt_bug.gb");
    
    //load_rom("../roms/assembly/timer_test.gb");
    load_rom("../roms/Tetris.gb");
}