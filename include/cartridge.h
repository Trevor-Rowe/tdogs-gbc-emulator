#ifndef CARTRIDGE_H
#define CARTRIDGE_H
#include<stdint.h>

typedef struct
{
    // ROM Memory info for banking purposes.
    unsigned long rom_size;
    uint16_t  total_rom_banks;
    uint16_t  current_rom_bank;

} rom_memory;

typedef struct
{
    // RAM Memory info for banking purposes.
    unsigned long ram_size;
    uint16_t total_ram_banks;
    uint16_t current_ram_bank;

} ram_memory;

// Inline with Pandocs specifications. 
typedef struct
{                       // Description             | [start - end)
    char title[15];     // Game Title              | 0x0134 - 0x0143
    /* 
    char m_code[4];     // Manufacturer Code       | 0x013F - 0x0143
    */
    uint8_t  cgb_flag;  // Enable Color Mode       | 0x0143 - 0x0144
    uint16_t nl_code;   // Game's publisher        | 0x0144 - 0x0146
    /*
    uint8_t sgb_flag;   // SGB function support?   | 0x0146 - 0x0147
    */
    uint8_t cart_type;  // Mapping schema          | 0x0147 - 0x0148
    /*
    uint8_t rom_size;   // Rom Size/# of Banks     | 0x0148 - 0x0149
    uint8_t ram_size;   // Ram Size (If applicable)| 0x0149 - 0x014A
    */
    uint8_t dest_code;  // Destination code        | 0x014A - 0x014B
    uint8_t ol_code;    // Old license code        | 0x014B - 0x014C
    uint8_t version;    // Version number          | 0x014C - 0x014D
    uint8_t checksum;   // Header checksum         | 0x014D - 0x014E

} rom_header;

int init_cartridge();

void print_cartridge_info();

void tidy_cartridge();

uint8_t read_rom_memory();

uint8_t write_rom_memory();

#endif