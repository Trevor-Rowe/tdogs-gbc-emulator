#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/*
    Reads in ROM and initializes cartridge context. 
    @param file_path   -> location of ROM file being opened
    @param testing     -> Indicates testing ROM. Skips header info and starts at 0x00.
    @return int (bool) -> Was initialization successful?
    @note              -> Call this function first. :)  
*/
int init_cartridge(char *file_path, uint16_t entry);

/*
    Prints header, rom, and ram information derived from the ROM file.
    @todo            -> Possible code refactor into samller print functions.
*/
void print_cartridge();

/*
    Cleans up memory and pointers that were used in a global context.
    @note           -> Call this only during program exit.
*/
void tidy_cartridge();

/*
    Reads byte from ROM content.
    @param address -> 16-Bit memory address
    @return uint8_t-> 8-Bit value from ROM memory
    @note 1        -> Takes into account current ROM/RAM bank.
    @note 2        -> Test with MBC and MBC1 first.
    @todo          -> Add support for larger ROM types. 
*/
uint8_t read_rom_memory(uint16_t address);

/*
    Handles ROM and RAM Memory Banking.
    @param address -> 16-Bit memory address
    @param value   -> 8-Bit value to 'write'
    @note          -> Emulates the effect of writing to the ROM without altering it.
                   -> Test with MBC and MBC1 first.
    @todo          -> Add support for larger ROM types. 
*/
void write_rom_memory(uint16_t address, uint8_t value);

/*
    @return        -> Address to begin ROM execution.
*/
uint16_t get_rom_start(); 

bool is_gbc();

#endif