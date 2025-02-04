#include <stdio.h>
#include <cartridge.h>
/*
    1. Cartridge
        a. memory banking
        b. different mbc support.
        c. read ROM into memory. 
        d. Collect ROM meta data.
    2, CPU Implementation
        a. CPU Stack (Start Pointer at 0x)
        b. Bus        (16-Bit)
        c. Memory     (0x0000 - 0xFFFF)
        d. Registers
        e. Clock      (~4.2 MHZ, delay conversion = (1/f)^-1 * 1000 ms/s)
        f. Operations (1 Byte OpCode + $CD Prefix Operations 512 total)
    3. Audio (Not yet researched)
    4. Graphics and PPU (Not yet researched)
*/
int main() 
{
    const char *file_path = "../roms/snake.gb";
    init_cartridge(file_path);
    print_cartridge_info();
    tidy_cartridge();
    print_cartridge_info();
    return 0;
}