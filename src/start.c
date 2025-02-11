#include "emulator.h"

int main() 
{   
    init_emulator("../roms/Tetris.gb", true);
    start_emulator();
    stop_emulator();
}