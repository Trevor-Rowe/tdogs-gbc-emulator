#include <stdlib.h>
#include "emulator.h"
#include "cart.h"
#include "cpu.h"
#include "logger.h"
#include "mmu.h"
#include "ppu.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

void tidy_emulator()
{
    tidy_memory();
    tidy_cpu();
    tidy_cartridge();
    tidy_graphics();
}

void init_emulator(char *file_path, uint16_t entry)
{
    LOG_MESSAGE(INFO, "Clock initialized.");
    init_memory(BUS_MEMORY_SIZE);
    LOG_MESSAGE(INFO, "Memory initialized.");
    init_cartridge(file_path, entry);
    LOG_MESSAGE(INFO, "Cartridge initialized.");
    print_cartridge();
    init_cpu();
    LOG_MESSAGE(INFO, "CPU initialized.");
    init_graphics();
    LOG_MESSAGE(INFO, "Graphics initialized.");
}