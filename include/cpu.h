#ifndef CPU_H
#define CPU_H
#include <stdbool.h>
#include <stdint.h>
#include "clock.h"

/* CONSTANTS FOR READABILITY */

typedef enum
{
    BASE_CLOCK_SPEED     = 4194304,
    DOUBLE_CLOCK_SPEED   = 8388608,
    BUS_MEMORY_SIZE      =  0xFFFF,
    DEFAULT_8BIT_REG_VAL =    0x00,
    MACHINE_TO_SYS_RATIO =       4 

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
    AF_REG = 0x00,
    BC_REG = 0x01,
    DE_REG = 0x02,
    HL_REG = 0x03,
    SP_REG = 0x04,
    PC_REG = 0x05

} DualRegister; 

/* STATE MANGEMENT */

typedef struct
{
    uint8_t   A; uint8_t   F; // Accumulator       | Flags
    uint8_t   B; uint8_t   C; // General Purpose   | -
    uint8_t   D; uint8_t   E; // General Purpose   | -
    uint8_t   H; uint8_t   L; // Memory Addressing | -
    uint16_t PC; uint16_t SP; // Program Counter   | Stack Pointer

} Register;

typedef struct
{
    uint8_t   ins_length;
    uint8_t   interrupt_enabled;
    bool      speed_enabled;
    bool      cpu_running;
    bool      testing;

} CPU;

/* 
    Executes the next machine cycle for the emulator.
    @return ->  Should the computer halt execution? 
*/
bool machine_cycle();

/* 
    Initializes CPU (Registers, Memory, Etc...)
    @return ->  Did initialization go correctly?
*/
bool init_cpu(bool testing);

/*
    Tidies up memory. 
    @note This should be the last call made when interacting with CPU.
*/
void tidy_cpu();

/*
    Resets program counter, specified by the loaded cartridge.
    @note Resets to 0x00 for testing purposes.
    @note 0x100 for GBC ROMS.
*/
void reset_pc();

void set_cpu_running(bool is_running);

/* 
    Gets pointer to CPU -TESTING ONLY!
*/
CPU *get_cpu_info();
Register *get_register_bank();
uint8_t *get_cpu_memory();
uint8_t get_ins_length();

#endif