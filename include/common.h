#ifndef COMMON_H
#define COMMON_H

typedef enum
{
    BIT_0_MASK      =  0b00000001,
    BIT_1_MASK      =  0b00000010,
    BIT_2_MASK      =  0b00000100,
    BIT_3_MASK      =  0b00001000,
    BIT_4_MASK      =  0b00010000,
    BIT_5_MASK      =  0b00100000,
    BIT_6_MASK      =  0b01000000,
    BIT_7_MASK      =  0b10000000,
    LOWER_2_MASK    =  0b00000011,
    LOWER_3_MASK    =  0b00000111,
    LOWER_4_MASK    =  0b00001111,
    LOWER_5_MASK    =  0b00011111, 
    LOWER_6_MASK    =  0b00111111,
    LOWER_12_MASK   =      0x0FFF,
    LOWER_14_MASK   =      0x3FFF,
    LOWER_BYTE_MASK =      0x00FF,
    UPPER_BYTE_MASK =      0xFF00,

} BitMask;

typedef enum
{
    BYTE           =       8,
    BYTE_OVERFLOW  =    0x00,
    BYTE_UNDERFLOW =    0xFF,
    IE_NEXT_INS    =       2,
    IS_ZERO        =    0x00,
    MAX_INT_16     =  0xFFFF,
    MAX_INT_8      =    0xFF,
    NIBBLE         =       4,

} CommonConstants;

typedef enum
{
    DOT_PER_FRAME      = 70224,
    GBC_WIDTH          =   160,
    GBC_HEIGHT         =   144,
    GRID_SIZE          =    32,
    OAM_SCAN_DELAY     =    80,
    OAM_ENTRY_SIZE     =     4,
    OBJ_PER_LINE       =    10,
    SCAN_LINE_QUANTITY =   154,
    TILE_SIZE          =     8,
    TILE_MAP_BANK_0    =     0,
    TILE_MAP_BANK_1    =     1,
    VBLANK_DELAY       =   456,

} GraphicDefaults;

#endif