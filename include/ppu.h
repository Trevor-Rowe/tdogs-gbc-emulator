#ifndef PPU_H
#define PPU_H

#define DOTS_PER_FRAME (uint32_t) 70224
#define DOTS_PER_LINE  (uint16_t)   456

typedef enum
{
    WHITE      = 0xFFE0F8D0,
    LIGHT_GRAY = 0xFF88C070,
    DARK_GRAY  = 0xFF346856,
    BLACK      = 0xFF081820

} DmgColors;

typedef enum
{
    HBLANK   = (uint8_t) 0x00, // (276 - Len(Mode 3))
    VBLANK   = (uint8_t) 0x01, // (4560 dots)
    OAM_SCAN = (uint8_t) 0x02, // (80 dots)
    DRAWING  = (uint8_t) 0x03  // (172-289 dots)

} PpuMode;

typedef enum
{
    B0_ADDRESS_START  = (uint16_t) 0x8000, // $8000
    B0_ADDRESS_END    = (uint16_t) 0x87FF, // $87FF
    B1_ADDRESS_START  = (uint16_t) 0x8800, // $8800
    B1_ADDRESS_END    = (uint16_t) 0x8FFF, // $8FFF
    B2_ADDRESS_START  = (uint16_t) 0x9000, // $9000
    B2_ADDRESS_END    = (uint16_t) 0x97FF, // $97FF
    TM0_ADDRESS_START = (uint16_t) 0x9800, // $9800
    TM0_ADDRESS_END   = (uint16_t) 0x9BFF, // $9BFF
    TM1_ADDRESS_START = (uint16_t) 0x9C00, // $9C00
    TM1_ADDRESS_END   = (uint16_t) 0x9FFF, // $9FFF
    TM_OFFEST         = (uint16_t) 0x0400  // $0400

} VRAMAddresses;

bool init_graphics();

void tidy_graphics();

void dot(uint32_t current_dot);

void *render_frame();

bool is_frame_ready();

#endif