#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>

#define DOTS_PER_FRAME (uint32_t) 70224
#define DOTS_PER_LINE  (uint16_t)   456

typedef enum
{
    B0_ADDRESS_START  = (uint16_t) 0x8000,
    B0_ADDRESS_END    = (uint16_t) 0x87FF,
    B1_ADDRESS_START  = (uint16_t) 0x8800,
    B1_ADDRESS_END    = (uint16_t) 0x8FFF,
    B2_ADDRESS_START  = (uint16_t) 0x9000,
    B2_ADDRESS_END    = (uint16_t) 0x97FF,
    TM0_ADDRESS_START = (uint16_t) 0x9800,
    TM0_ADDRESS_END   = (uint16_t) 0x9BFF,
    TM1_ADDRESS_START = (uint16_t) 0x9C00,
    TM1_ADDRESS_END   = (uint16_t) 0x9FFF,
    TM_OFFEST         = (uint16_t) 0x0400

} VRAMAddresses;

typedef enum
{
    HBLANK   = (uint8_t) 0x00, // (276 - Len(Mode 3))
    VBLANK   = (uint8_t) 0x01, // (4560 dots)
    OAM_SCAN = (uint8_t) 0x02, // (80 dots)
    DRAWING  = (uint8_t) 0x03  // (172-289 dots)

} PpuMode;

bool init_graphics();

void tidy_graphics();

void dot(uint32_t current_dot);

void *render_frame();

bool is_frame_ready();

#endif