#ifndef CPU_H
#define CPU_H
#include <stdbool.h>
#include <stdint.h>


typedef enum
{
    BASE_CLOCK_SPEED     = 4194304,
    BUS_MEMORY_SIZE      =  0xFFFF,
    DEFAULT_8BIT_REG_VAL =    0x00,
    M2S_BASE_SPEED       =       4,
    M2S_DOUBLE_SPEED     =       2,
    M2S_RATIO            =       4,

} CpuDefaults;

typedef enum
{
    ZERO_FLAG       = 0b10000000,
    SUBTRACT_FLAG   = 0b01000000,
    HALF_CARRY_FLAG = 0b00100000,
    CARRY_FLAG      = 0b00010000 
      
} Flag;

typedef enum
{
    VBLANK_INTERRUPT_CODE   = 0x01,
    LCD_STAT_INTERRUPT_CODE = 0x02,
    TIMER_INTERRUPT_CODE    = 0x04,
    SERIAL_INTERRUPT_CODE   = 0x08,
    JOYPAD_INTERRUPT_CODE   = 0x10
    
} InterruptCodes;

/* STATE MANGEMENT */

typedef struct
{
    uint8_t   A; uint8_t   F; // Accumulator       | Flags
    uint8_t   B; uint8_t   C; // General Purpose   | -
    uint8_t   D; uint8_t   E; // General Purpose   | -
    uint8_t   H; uint8_t   L; // Memory Addressing | -
    uint16_t PC; uint16_t SP; // Program Counter   | Stack Pointer

} Register;

uint8_t machine_cycle();

bool init_cpu();

void tidy_cpu();

void reset_pc();

void start_cpu();

void stop_cpu();

bool is_flag_set(Flag flag);

bool is_speed_enabled();

bool cpu_running();

Register *get_register_bank();

uint8_t get_ins_length();

void request_interrupt(InterruptCodes interrupt);

#endif