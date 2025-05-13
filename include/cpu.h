#ifndef CPU_H
#define CPU_H
#include <stdint.h>

typedef enum
{
    BASE_CLOCK_SPEED     = 4194304,
    DEFAULT_REG_VAL      =       0,
    M2S_BASE_SPEED       =       4,
    M2S_DOUBLE_SPEED     =       2,
    DIV_INC_PERIOD       =      64,

} CpuDefaults;

typedef enum
{
    ZERO_FLAG       = 0b10000000, // Z
    SUBTRACT_FLAG   = 0b01000000, // N
    HALF_CARRY_FLAG = 0b00100000, // H
    CARRY_FLAG      = 0b00010000  // C
      
} Flag;

typedef enum
{
    VBLANK_INTERRUPT_CODE   = 0x01,
    LCD_STAT_INTERRUPT_CODE = 0x02,
    TIMER_INTERRUPT_CODE    = 0x04,
    SERIAL_INTERRUPT_CODE   = 0x08,
    JOYPAD_INTERRUPT_CODE   = 0x10
    
} InterruptCode;

/* STATE MANGEMENT */

typedef struct
{
    // CPU Registers
    uint8_t     A; uint8_t      F; // Accumulator          | Flags
    uint8_t     B; uint8_t      C; // General Purpose      | -
    uint8_t     D; uint8_t      E; // General Purpose      | -
    uint8_t     H; uint8_t      L; // Memory Addressing    | -
    uint16_t   PC; uint16_t    SP; // Program Counter      | Stack Pointer
    // Hardware Registers
    uint8_t  *IER; uint8_t   *IFR;

} Register;

uint8_t get_machine_cycle_scaler();

void machine_cycle();

void init_cpu();

void tidy_cpu();

void reset_cpu();

void start_cpu();

void stop_cpu();

bool is_speed_enabled();

bool cpu_running();

void request_interrupt(InterruptCode interrupt);

char *get_cpu_state(char *buffer, size_t size);

void write_ifr(uint8_t value);

#endif