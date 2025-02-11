#ifndef COMMON_H
#define COMMON_H

typedef enum
{
    LOWER_12_MASK   = 0x0FFF,
    LOWER_5_MASK    = 0b00011111, // 0x1F
    LOWER_4_MASK    = 0b00001111, // 0x0F
    LOWER_2_MASK    = 0b00000011, // 0x03
    BIT_0_MASK      = 0b00000001,  // 0x01
    BIT_1_MASK      = 0b00000010,
    BIT_2_MASK      = 0b00000100,
    BIT_3_MASK      = 0b00001000,
    BIT_4_MASK      = 0b00010000,
    BIT_5_MASK      = 0b00100000,
    BIT_6_MASK      = 0b01000000,
    BIT_7_MASK      = 0b10000000,  // 0x80
    LOWER_BYTE_MASK = 0x00FF,
    UPPER_BYTE_MASK = 0xFF00
    
} BitMask;

typedef enum
{
    BANK_ZERO_ADDRESS        = 0x4000,
    BANK_N_ADDRESS           = 0x7FFF,
    VRAM_ADDRESS             = 0x9FFF,
    EXT_RAM_ADDRESS          = 0xBFFF,
    WRAM_ZERO_ADDRESS        = 0xCFFF,
    WRAM_N_ADDRESS           = 0xDFFF,
    ECHO_RAM_ADDRESS         = 0xFDFF,
    OAM_ADDRESS              = 0xFE9F,
    NOT_USABLE               = 0xFEFF,
    IO_REGISTERS_START       = 0xFF00,
    IO_REGISTERS_END         = 0xFF7F,
    HIGH_RAM_ADDRESS_START   = 0xFF80,
    HIGH_RAM_ADDRESS_END     = 0xFFFE,
    INTERRUPT_ENABLE_ADDRESS = 0xFFFF

} Addresses;

typedef enum
{
    BYTE           = 8,
    NIBBLE         = 4,
    BYTE_OVERFLOW  = 0x00,
    BYTE_UNDERFLOW = 0xFF,
    IS_ZERO        = 0x00,
    MAX_INT_16     = 0xFFFF,
    MAX_INT_8      = 0xFF,
    IE_NEXT_INS    = 2 

} CommonConstants;

#endif