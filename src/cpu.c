#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> 
#include <string.h>
#include "common.h"
#include "cpu.h"
#include "cart.h"
#include "disassembler.h"
#include "logger.h"
#include "mmu.h"
#include "timer.h"

#define STR_BUFFER_SIZE 128
#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

typedef struct InstructionEntity
{
    uint16_t  address;
    uint8_t  duration;
    uint8_t    length;
    uint8_t       low;
    uint8_t      high;
    uint8_t    opcode;
    char       *label;
    bool     executed;

    bool (*handler)(struct InstructionEntity*);

} InstructionEntity;

typedef bool (*OpcodeHandler)(InstructionEntity*);

/* CONSTANTS FOR READABILITY */

typedef enum
{
    AF_REG = 0x00,
    BC_REG = 0x01,
    DE_REG = 0x02,
    HL_REG = 0x03,
    SP_REG = 0x04

} DualRegister;

typedef enum
{
    RISING  = 0, 
    FALLING = 1
    
} EdgeState;

/* STATE MANGEMENT */

typedef struct
{
    bool              ime;
    bool    speed_enabled;
    bool          running;
    bool           halted;
    bool  halt_bug_active;

} CPU;

typedef struct
{
    uint8_t delay;
    bool   active;

} InterruptEnableEvent;

/* GLOBAL STATE POINTERS */

static CPU                  *cpu;
static Register               *R;
static InterruptEnableEvent *iee;
static InstructionEntity    *ins;
static bool          cb_prefixed;

/* STATIC (PRIVATE) HELPERS FUNCTIONS */

static uint16_t form_address(InstructionEntity *ins)
{ 
    uint16_t   upper = ins->high;
    uint16_t   lower = ins->low;
    uint16_t address = (upper << BYTE) |  lower;
    return address;
}

static void write_flag_reg(uint8_t value)
{
    R->F = (value & 0xF0); // Only the upper nibble.
}

static void set_flag(bool is_set, Flag flag_mask)
{
    uint8_t value = is_set ? (R->F | flag_mask) : (R->F & ~flag_mask);
    write_flag_reg(value);
}

static bool is_flag_set(Flag flag)
{
    return ((R->F & flag) != 0);
}

static uint8_t fetch()
{ // Fetch next instruction to be executed.
    uint8_t rom_byte = 0x00; // Jusssssst in case...
    if (cpu->halt_bug_active)
    {
        cpu->halt_bug_active = false;
        rom_byte = read_memory(R->PC);
        cpu_log(DEBUG, "Halt Bug Fetch %02X", rom_byte);
    }
    else
    {
        rom_byte = read_memory(R->PC++);
    }
    return rom_byte;
}

static uint16_t getDR(DualRegister dr)
{
    switch(dr)
    {
        case AF_REG: return (uint16_t)((R->A << BYTE) | R->F);
        case BC_REG: return (uint16_t)((R->B << BYTE) | R->C);
        case DE_REG: return (uint16_t)((R->D << BYTE) | R->E);
        case HL_REG: return (uint16_t)((R->H << BYTE) | R->L);
        case SP_REG: return R->SP;
        default: 
            LOG_MESSAGE(ERROR, "Dual Register Not Selected");
            return 0;
    }
}

static void setDR(DualRegister dr, uint16_t source)
{
    switch(dr)
    {
        case AF_REG:
            R->A = ((uint8_t) (source >> BYTE));
            R->F = ((uint8_t) (source & 0xF0));            
            break;
        case BC_REG:
            R->B = ((uint8_t) (source >> BYTE));
            R->C = ((uint8_t) source & 0xFF);
            break;
        case DE_REG:
            R->D = ((uint8_t) (source >> BYTE));
            R->E = ((uint8_t) source & 0xFF);
            break;
        case HL_REG:
            R->H = ((uint8_t) (source >> BYTE));
            R->L = ((uint8_t) source & 0xFF);
            break;
        case SP_REG:
            R->SP = source;
            break;
        default:
            LOG_MESSAGE(ERROR, "Invalid Dual Register for setDR()");
            break;
    }
}

static uint8_t pop_stack()
{
    uint8_t result = read_memory(R->SP);
    R->SP += 1; 
    return result;
}

static void push_stack(uint8_t value)
{
    R->SP -= 1;
    write_memory(R->SP, value);
}

static void schedule_ime(InterruptEnableEvent *iee)
{
    iee->delay  = 2;
    iee->active = true;
}

static uint8_t get_pending_interrupts()
{
    uint8_t ifr = *R->IFR & LOWER_5_MASK;
    uint8_t ier = *R->IER & LOWER_5_MASK;
    return ifr & ier;
}

void write_ifr(uint8_t value)
{
    (*R->IFR) = (0xE0 | (value & LOWER_5_MASK));
}

char *get_cpu_state(char *buffer, size_t size)
{
    snprintf(
        buffer, 
        size,
        "IME-%d | PC-$%04X | SP-$%04X | INT-($%02X & $%02X : $%02X) ||$%02X|| - %-17s ->",
        cpu->ime, 
        R->PC, 
        R->SP,
        (*R->IER),
        (*R->IFR),
        get_pending_interrupts(), 
        ins->opcode, 
        ins->label
    );
    
    // snprintf(
    //     buffer,
    //     size,
    //     "A-$%02X%02X-F || B-$%02X%02X-C || D-$%02X%02X-E || H-$%02X%02X-L [PC=%04X] $%02X- %-17s ->",
    //     R->A,   R->F,  R->B,  R->C,  R->D,  R->E,  R->H,  R->L,  R->PC, ins->opcode, ins->label
    // );
    
    return buffer;
}

/* CPU OPCODE IMPLEMENTATION  */

// Checked
static bool nop(InstructionEntity *ins)        // 0x00 (- - - -) 1M
{
    cpu_log(DEBUG, "...");
    return true;
}
static bool halt(InstructionEntity *ins)       // 0x76 (- - - -) 1M
{
    uint8_t pending = get_pending_interrupts();

    if ((cpu->ime == 0) && (pending != 0))
    {
        cpu->halt_bug_active = true;
        cpu->halted = false;
        cpu_log(DEBUG, "Halt Bug!");
    }
    else
    {
        cpu->halted = (pending == 0);
        cpu_log(DEBUG, "Halt set based on pending interrupts.");
    }

    return true;
}

static bool stop(InstructionEntity *ins) // 0x10
{
    fetch(); // STOP is a 2-byte instruction; second byte is unused

    uint8_t key1 = read_memory(KEY1);
    clear_sys();

    if (is_gbc() && ((key1 & BIT_0_MASK) != cpu->speed_enabled)) // Prepare Speed Switch set
    {
        cpu->speed_enabled = !cpu->speed_enabled;

        // Set bit 7 = speed, clear bit 0 = handshake complete
        uint8_t new_key1 = (cpu->speed_enabled << 7);
        write_memory(KEY1, new_key1);

        cpu_log(DEBUG, "Speed Mode toggled to: %d", cpu->speed_enabled);
        return true;
    }

    cpu_log(DEBUG, "STOP executed without speed toggle.");
    return true;
}

// Checked
static bool ld_bc_nn(InstructionEntity *ins)   // 0x01 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->C = fetch();
        cpu_log(DEBUG, "Loaded $%02X into C", R->C);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->B = fetch();
        cpu_log(DEBUG, "Loaded $%02X into B", R->B);
        return true; // Instruction complete
    }

    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_de_nn(InstructionEntity *ins)   // 0x11 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->E = fetch();
        cpu_log(DEBUG, "Loaded $%02X into E", R->E);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->D = fetch();
        cpu_log(DEBUG, "Loaded $%02X into D", R->D);
        return true; // Instruction complete
    }

    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_hl_nn(InstructionEntity *ins)   // 0x21 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->L = fetch();
        cpu_log(DEBUG, "Loaded $%02X into L", R->L);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->H = fetch();
        cpu_log(DEBUG, "Loaded $%02X into H", R->H);
        return true; // Instruction complete
    }

    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_sp_nn(InstructionEntity *ins)   // 0x31 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched and Loaded byte $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->   high = fetch();
        ins->address = form_address(ins);
        R->SP = ins->address;
        cpu_log(DEBUG, "Fetched and Loaded byte $%02X", ins->high);
        return true; // Instruction complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static void reg_inc_16(DualRegister dr)
{
    switch(dr)
    {
        case BC_REG: 
            R->C += 1;
            if (R->C == 0) R->B += 1; 
            break;
        case DE_REG: 
            R->E += 1;
            if (R->E == 0) R->D += 1; 
            break;
        case HL_REG:
            R->L += 1;
            if (R->L == 0) R->H += 1; 
            break;
        case SP_REG:
            R->SP += 1;
            break;
    }
}
static bool reg_inc_16_handler(InstructionEntity *ins, DualRegister dr)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        reg_inc_16(dr);
        cpu_log(DEBUG, "Incremented $%04X", getDR(dr));
        return true; // Instruction Complete
    }

    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool inc_bc(InstructionEntity *ins)     // 0x03 (- - - -) 2M
{
    return reg_inc_16_handler(ins, BC_REG);
}
static bool inc_de(InstructionEntity *ins)     // 0x13 (- - - -) 2M
{ 
    return reg_inc_16_handler(ins, DE_REG);
}
static bool inc_hl(InstructionEntity *ins)     // 0x23 (- - - -) 2M
{
    return reg_inc_16_handler(ins, HL_REG);
}
static bool inc_sp(InstructionEntity *ins)     // 0x33 (- - - -) 2M
{ 
    return reg_inc_16_handler(ins, SP_REG);
}
// Checked
static void reg_dec_16(DualRegister dr)
{
    switch(dr)
    {
        case BC_REG:
            R->C -= 1;
            if (R->C == BYTE_UNDERFLOW) R->B -= 1; 
            break;
        case DE_REG: 
            R->E -= 1;
            if (R->E == BYTE_UNDERFLOW) R->D -= 1;
            break;
        case HL_REG:
            R->L -= 1;
            if (R->L == BYTE_UNDERFLOW) R->H -= 1;
            break;
        case SP_REG:
            R->SP -= 1;
            break;
    }
}
static bool reg_dec_16_handler(InstructionEntity *ins, DualRegister dr)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        reg_dec_16(dr);
        cpu_log(DEBUG, "Decremented $%04X", getDR(dr));
        return true; // Instruction Complete
    }

    cpu_log(ERROR, "Invalid operation, moving on.");
    return false;
}
static bool dec_bc(InstructionEntity *ins)     // 0x0B (- - - -) 2M
{
    reg_dec_16_handler(ins, BC_REG);
}
static bool dec_de(InstructionEntity *ins)     // 0x1B (- - - -) 2M
{
    reg_dec_16_handler(ins, DE_REG);
}
static bool dec_hl(InstructionEntity *ins)     // 0x2B (- - - -) 2M
{
    reg_dec_16_handler(ins, HL_REG);
}
static bool dec_sp(InstructionEntity *ins)     // 0x3B (- - - -) 2M
{ 
    reg_dec_16_handler(ins, SP_REG);
}
// Checked
static bool pop_bc(InstructionEntity *ins)     // 0xC1 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->C = pop_stack();
        cpu_log(DEBUG, "Popped $%02X into C", R->C);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->B = pop_stack();
        cpu_log(DEBUG, "Popped $%02X into B", R->B);
        return true; // Instruction complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool pop_de(InstructionEntity *ins)     // 0xD1 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->E = pop_stack();
        cpu_log(DEBUG, "Popped $%02X into E", R->E);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->D = pop_stack();
        cpu_log(DEBUG, "Popped $%02X into D", R->D);
        return true; // Instruction complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool pop_hl(InstructionEntity *ins)     // 0xE1 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->L = pop_stack();
        cpu_log(DEBUG, "Popped $%02X into L", R->L);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->H = pop_stack();
        cpu_log(DEBUG, "Popped $%02X into H", R->H);
        return true; // Instruction complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool pop_af(InstructionEntity *ins)     // 0xF1 (Z N H C) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        write_flag_reg(pop_stack());
        cpu_log(DEBUG, "Popped $%02X into F", R->F);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->A = pop_stack();
        cpu_log(DEBUG, "Popped $%02X into A", R->A);
        return true; // Instruction complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool push_bc(InstructionEntity *ins)    // 0xC5 (- - - -) 4M
{
    if (ins->duration <= 2) // First - Second Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        push_stack(R->B);
        cpu_log(DEBUG, "Pushed B-$%02X onto stack", R->B);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        push_stack(R->C);
        cpu_log(DEBUG, "Pushed C-$%02X onto stack", R->C);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool push_de(InstructionEntity *ins)    // 0xD5 (- - - -) 4M
{
    if (ins->duration <= 2) // First - Second Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        push_stack(R->D);
        cpu_log(DEBUG, "Pushed D-$%02X onto stack", R->D);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        push_stack(R->E);
        cpu_log(DEBUG, "Pushed E-$%02X onto stack", R->E);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool push_hl(InstructionEntity *ins)    // 0xE5 (- - - -) 4M
{
    if (ins->duration <= 2) // First - Second Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        push_stack(R->H);
        cpu_log(DEBUG, "Pushed H-$%02X onto stack", R->H);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        push_stack(R->L);
        cpu_log(DEBUG, "Pushed L-$%02X onto stack", R->L);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool push_af(InstructionEntity *ins)    // 0xF5 (- - - -) 4M
{
    if (ins->duration <= 2) // First - Second Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        push_stack(R->A);
        cpu_log(DEBUG, "Pushed A-$%02X onto stack", R->A);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        push_stack(R->F);
        cpu_log(DEBUG, "Pushed F-$%02X onto stack", R->F);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool ld_hli_a(InstructionEntity *ins)   // 0x22 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        write_memory(hl, R->A);
        setDR(HL_REG, (hl + 1));
        cpu_log(DEBUG, "Wrote $%02X into [$%04X], Incremented", R->A, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_a_hli(InstructionEntity *ins)   // 0x2A (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->A = read_memory(hl);
        setDR(HL_REG, (hl + 1));
        cpu_log(DEBUG, "Loaded $%02X from [$%04X], incremented", R->A, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_hld_a(InstructionEntity *ins)   // 0x32 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        write_memory(hl, R->A);
        setDR(HL_REG, (hl - 1));
        cpu_log(DEBUG, "Wrote $%02X into [$%04X], decremented", R->A, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_a_hld(InstructionEntity *ins)   // 0x3A (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->A = read_memory(hl);
        setDR(HL_REG, (hl - 1));
        cpu_log(DEBUG, "Loaded $%02X from [$%04X], decremented", R->A, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool ld_bc_a(InstructionEntity *ins)    // 0x02 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t bc = getDR(BC_REG);
        write_memory(bc, R->A);
        cpu_log(DEBUG, "Loaded $%02X into [$%04X]", R->A, bc);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_de_a(InstructionEntity *ins)    // 0x12 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t de = getDR(DE_REG);
        write_memory(de, R->A);
        cpu_log(DEBUG, "Loaded $%02X into [$%04X]", R->A, de);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_nn_sp(InstructionEntity *ins)   // 0x08 (- - - -) 5M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched byte $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->   high = fetch();
        ins->address = form_address(ins);
        cpu_log(DEBUG, "Fetched byte $%02X", ins->high);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        uint8_t sp_low = R->SP & LOWER_BYTE_MASK;
        write_memory(ins->address, sp_low);
        cpu_log(DEBUG, "Wrote $%02X into [$%04X]", sp_low, ins->address);
        return false;
    }

    if (ins->duration == 5) // Fifth Cycle
    {
        uint8_t sp_high = (R->SP >> BYTE) & LOWER_BYTE_MASK;
        write_memory(ins->address + 1, sp_high);
        cpu_log(DEBUG, "Wrote $%02X into [$%04X]", sp_high, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool ld_hl_reg(InstructionEntity *ins, uint8_t reg)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        write_memory(hl, reg);
        cpu_log(DEBUG, "Wrote $%02X into [$%04X]", reg, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_hl_b(InstructionEntity *ins)    // 0x70 (- - - -) 2M
{
    return ld_hl_reg(ins, R->B);
}
static bool ld_hl_c(InstructionEntity *ins)    // 0x71 (- - - -) 2M
{
    return ld_hl_reg(ins, R->C);
}
static bool ld_hl_d(InstructionEntity *ins)    // 0x72 (- - - -) 2M
{
    return ld_hl_reg(ins, R->D);
}
static bool ld_hl_e(InstructionEntity *ins)    // 0x63 (- - - -) 2M
{
    return ld_hl_reg(ins, R->E);
}
static bool ld_hl_h(InstructionEntity *ins)    // 0x64 (- - - -) 2M
{
    return ld_hl_reg(ins, R->H);
}
static bool ld_hl_l(InstructionEntity *ins)    // 0x65 (- - - -) 2M
{
    return ld_hl_reg(ins, R->L);
}
static bool ld_hl_a(InstructionEntity *ins)    // 0x77 (- - - -) 2M
{
    return ld_hl_reg(ins, R->A);
}
// Checked
static bool ld_b_c(InstructionEntity *ins)     // 0x41 (- - - -) 1M
{
    R->B = R->C;
    cpu_log(DEBUG, "Loaded $%02X", R->B);
    return true;
}
static bool ld_b_d(InstructionEntity *ins)     // 0x42 (- - - -) 1M
{
    R->B = R->D;
    cpu_log(DEBUG, "Loaded $%02X", R->B);
    return true;
}
static bool ld_b_e(InstructionEntity *ins)     // 0x43 (- - - -) 1M
{
    R->B = R->E;
    cpu_log(DEBUG, "Loaded $%02X", R->B);
    return true;
}
static bool ld_b_h(InstructionEntity *ins)     // 0x44 (- - - -) 1M
{
    R->B = R->H;
    cpu_log(DEBUG, "Loaded $%02X", R->B);
    return true;
}
static bool ld_b_l(InstructionEntity *ins)     // 0x45 (- - - -) 1M
{
    R->B = R->L;
    cpu_log(DEBUG, "Loaded $%02X", R->B);
    return true;
}
static bool ld_b_hl(InstructionEntity *ins)    // 0x46 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->B = read_memory(hl);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->B, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_b_a(InstructionEntity *ins)     // 0x47 (- - - -) 1M
{
    R->B = R->A;
    cpu_log(DEBUG, "Loaded $%02X", R->B);
    return true;
}
static bool ld_c_b(InstructionEntity *ins)     // 0x48 (- - - -) 1M
{
    R->C = R->B;
    cpu_log(DEBUG, "Loaded $%02X", R->C);
    return true;
}
static bool ld_c_d(InstructionEntity *ins)     // 0x4A (- - - -) 1M
{
    R->C = R->D;
    cpu_log(DEBUG, "Loaded $%02X", R->C);
    return true;
}
static bool ld_c_e(InstructionEntity *ins)     // 0x4B (- - - -) 1M
{
    R->C = R->E;
    cpu_log(DEBUG, "Loaded $%02X", R->C);
    return true;
}
static bool ld_c_h(InstructionEntity *ins)     // 0x4C (- - - -) 1M
{
    R->C = R->H;
    cpu_log(DEBUG, "Loaded $%02X", R->C);
    return true;
}
static bool ld_c_l(InstructionEntity *ins)     // 0x4D (- - - -) 1M
{
    R->C = R->L;
    cpu_log(DEBUG, "Loaded $%02X", R->C);
    return true;
}
static bool ld_c_hl(InstructionEntity *ins)    // 0x4E (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->C = read_memory(hl);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->C, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_c_a(InstructionEntity *ins)     // 0x4F (- - - -) 1M
{
    R->C = R->A;
    cpu_log(DEBUG, "Loaded $%02X", R->C);
    return true;
}
// Checked
static bool ld_d_b(InstructionEntity *ins)     // 0x50 (- - - -) 1M
{
    R->D = R->B;
    cpu_log(DEBUG, "Loaded $%02X", R->D);
    return true;
}
static bool ld_d_c(InstructionEntity *ins)     // 0x51 (- - - -) 1M
{
    R->D = R->C;
    cpu_log(DEBUG, "Loaded $%02X", R->D);
    return true;
}
static bool ld_d_e(InstructionEntity *ins)     // 0x53 (- - - -) 1M
{
    R->D = R->E;
    cpu_log(DEBUG, "Loaded $%02X", R->D);
    return true;
}
static bool ld_d_h(InstructionEntity *ins)     // 0x54 (- - - -) 1M
{
    R->D = R->H;
    cpu_log(DEBUG, "Loaded $%02X", R->D);
    return true;
}
static bool ld_d_l(InstructionEntity *ins)     // 0x55 (- - - -) 1M
{
    R->D = R->L;
    cpu_log(DEBUG, "Loaded $%02X", R->D);
    return true;
}
static bool ld_d_hl(InstructionEntity *ins)    // 0x56 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->D = read_memory(hl);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->D, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_d_a(InstructionEntity *ins)     // 0x57 (- - - -) 1M
{
    R->D = R->A;
    cpu_log(DEBUG, "Loaded $%02X", R->D);
    return true;
}
static bool ld_e_b(InstructionEntity *ins)     // 0x58 (- - - -) 1M
{
    R->E = R->B;
    cpu_log(DEBUG, "Loaded $%02X", R->E);
    return true;
}
static bool ld_e_c(InstructionEntity *ins)     // 0x59 (- - - -) 1M
{
    R->E = R->C;
    cpu_log(DEBUG, "Loaded $%02X", R->E);
    return true;
}
static bool ld_e_d(InstructionEntity *ins)     // 0x5A (- - - -) 1M
{
    R->E = R->D;
    cpu_log(DEBUG, "Loaded $%02X", R->E);
    return true;
}
static bool ld_e_h(InstructionEntity *ins)     // 0x5C (- - - -) 1M
{
    R->E = R->H;
    cpu_log(DEBUG, "Loaded $%02X", R->E);
    return true;
}
static bool ld_e_l(InstructionEntity *ins)     // 0x5D (- - - -) 1M
{
    R->E = R->L;
    cpu_log(DEBUG, "Loaded $%02X", R->E);
    return true;
}
static bool ld_e_hl(InstructionEntity *ins)    // 0x5E (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->E = read_memory(hl);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->E, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_e_a(InstructionEntity *ins)     // 0x5F (- - - -) 1M
{
    R->E = R->A;
    cpu_log(DEBUG, "Loaded $%02X", R->E);
    return true;
}
// Checked
static bool ld_h_b(InstructionEntity *ins)     // 0x60 (- - - -) 1M
{
    R->H = R->B;
    cpu_log(DEBUG, "Loaded $%02X", R->H);
    return true;
}
static bool ld_h_c(InstructionEntity *ins)     // 0x61 (- - - -) 1M)
{
    R->H = R->C;
    cpu_log(DEBUG, "Loaded $%02X", R->H);
    return true;
}
static bool ld_h_d(InstructionEntity *ins)     // 0x62 (- - - -) 1M
{
    R->H = R->D;
    cpu_log(DEBUG, "Loaded $%02X", R->H);
    return true;
}
static bool ld_h_e(InstructionEntity *ins)     // 0x63 (- - - -) 1M
{
    R->H = R->E;
    cpu_log(DEBUG, "Loaded $%02X", R->H);
    return true;
}
static bool ld_h_l(InstructionEntity *ins)     // 0x65 (- - - -) 1M
{
    R->H = R->L;
    cpu_log(DEBUG, "Loaded $%02X", R->H);
    return true;
}
static bool ld_h_hl(InstructionEntity *ins)    // 0x66 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->H = read_memory(hl);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->H, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_h_a(InstructionEntity *ins)     // 0x67 (- - - -) 1M
{
    R->H = R->A;
    cpu_log(DEBUG, "Loaded $%02X", R->H);
    return true;
}
static bool ld_l_b(InstructionEntity *ins)     // 0x68 (- - - -) 1M
{
    R->L = R->B;
    cpu_log(DEBUG, "Loaded $%02X", R->L);
    return true;
}
static bool ld_l_c(InstructionEntity *ins)     // 0x69 (- - - -) 1M
{
    R->L = R->C;
    cpu_log(DEBUG, "Loaded $%02X", R->L);
    return true;
}
static bool ld_l_d(InstructionEntity *ins)     // 0x6A (- - - -) 1M
{
    R->L = R->D;
    cpu_log(DEBUG, "Loaded $%02X", R->L);
    return true;
}
static bool ld_l_e(InstructionEntity *ins)     // 0x6B (- - - -) 1M
{
    R->L = R->E;
    cpu_log(DEBUG, "Loaded $%02X", R->L);
    return true;
}
static bool ld_l_h(InstructionEntity *ins)     // 0x6C (- - - -) 1M
{
    R->L = R->H;
    cpu_log(DEBUG, "Loaded $%02X", R->L);
    return true;
}
static bool ld_l_hl(InstructionEntity *ins)    // 0x6E (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->L = read_memory(hl);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->L, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_l_a(InstructionEntity *ins)     // 0x6F (- - - -) 1M
{ 
    R->L = R->A;
    cpu_log(DEBUG, "Loaded $%02X", R->L);
    return true;
}
// Checked
static bool ld_a_b(InstructionEntity *ins)     // 0x78 (- - - -) 1M
{
    R->A = R->B;
    cpu_log(DEBUG, "Loaded $%02X", R->A);
    return true;
}
static bool ld_a_c(InstructionEntity *ins)     // 0x79 (- - - -) 1M
{ 
    R->A = R->C;
    cpu_log(DEBUG, "Loaded $%02X", R->A);
    return true;
}
static bool ld_a_d(InstructionEntity *ins)     // 0x7A (- - - -) 1M
{
    R->A = R->D;
    cpu_log(DEBUG, "Loaded $%02X", R->A);
    return true;
}
static bool ld_a_e(InstructionEntity *ins)     // 0x7B (- - - -) 1M
{
    R->A = R->E;
    cpu_log(DEBUG, "Loaded $%02X", R->A);
    return true;
}
static bool ld_a_h(InstructionEntity *ins)     // 0x7C (- - - -) 1M
{
    R->A = R->H;
    cpu_log(DEBUG, "Loaded $%02X", R->A);
    return true;
}
static bool ld_a_l(InstructionEntity *ins)     // 0x7D (- - - -) 1M
{
    R->A = R->L;
    cpu_log(DEBUG, "Loaded $%02X", R->A);
    return true;
}
static bool ld_a_hl(InstructionEntity *ins)    // 0x7E (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        R->A = read_memory(hl);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->A, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_a_bc(InstructionEntity *ins)    // 0x0A (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t bc = getDR(BC_REG);
        R->A = read_memory(bc);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->A, bc);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_a_de(InstructionEntity *ins)    // 0x1A (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t de = getDR(DE_REG);
        R->A = read_memory(de);
        cpu_log(DEBUG, "Loaded $%02X from [$%04X]", R->A, de);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool reg_ld_n_8(InstructionEntity *ins, uint8_t *reg)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        *reg = fetch();
        cpu_log(DEBUG, "Loaded $%02X", *reg);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_b_n(InstructionEntity *ins)     // 0x06 (- - - -) 2M
{
    return reg_ld_n_8(ins, &(R->B));
}
static bool ld_c_n(InstructionEntity *ins)     // 0x0E (- - - -) 2M
{
    return reg_ld_n_8(ins, &(R->C));
}
static bool ld_d_n(InstructionEntity *ins)     // 0x16 (- - - -) 2M
{
    return reg_ld_n_8(ins, &(R->D));
}
static bool ld_e_n(InstructionEntity *ins)     // 0x1E (- - - -) 2M
{
    return reg_ld_n_8(ins, &(R->E));
}
static bool ld_h_n(InstructionEntity *ins)     // 0x26 (- - - -) 2M
{
    return reg_ld_n_8(ins, &(R->H));
}
static bool ld_l_n(InstructionEntity *ins)     // 0x2E (- - - -) 2M
{
    return reg_ld_n_8(ins, &(R->L));
}
static bool ld_hl_n(InstructionEntity *ins)    // 0x36 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched byte $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint16_t hl = getDR(HL_REG);
        write_memory(hl, ins->low);
        cpu_log(DEBUG, "Wrote $%02X into [$%04X]", ins->low, hl);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_a_n(InstructionEntity *ins)     // 0x3E (- - - -) 2M
{
    return reg_ld_n_8(ins, &(R->A));
}
// Checked
static uint8_t reg_inc_8(uint8_t r)
{
    uint8_t result = r + 1;
    bool   is_zero = (result == 0);
    bool hc_exists = ((r & LOWER_4_MASK) == LOWER_4_MASK);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    return result;
}
static bool inc_b(InstructionEntity *ins)      // 0x04 (Z 0 H -) 1M
{
    R->B = reg_inc_8(R->B);
    cpu_log(DEBUG, "Incremented $%02X", R->B);
    return true;
}
static bool inc_d(InstructionEntity *ins)      // 0x14 (Z 0 H -) 1M
{
    R->D = reg_inc_8(R->D);
    cpu_log(DEBUG, "Incremented $%02X", R->D);
    return true;
}
static bool inc_h(InstructionEntity *ins)      // 0x24 (Z 0 H -) 1M
{
    R->H = reg_inc_8(R->H);
    cpu_log(DEBUG, "Incremented $%02X", R->H);
    return true;
}
static bool inc_hl_mem(InstructionEntity *ins) // 0x34 (Z 0 H -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        ins->address = hl;
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, hl);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_inc_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Incremented [$%04X] - $%02X", ins->address, result);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool inc_c(InstructionEntity *ins)      // 0x0C (Z 0 H -) 1M
{
    R->C = reg_inc_8(R->C);
    cpu_log(DEBUG, "Incremented $%02X", R->C);
    return true;
}
static bool inc_e(InstructionEntity *ins)      // 0x1C (Z 0 H -) 1M
{
    R->E = reg_inc_8(R->E);
    cpu_log(DEBUG, "Incremented $%02X", R->E);
    return true;
}
static bool inc_l(InstructionEntity *ins)      // 0x2C (Z 0 H -) 1M
{
    R->L = reg_inc_8(R->L);
    cpu_log(DEBUG, "Incremented $%02X", R->L);
    return true;
}
static bool inc_a(InstructionEntity *ins)      // 0x3C (Z 0 H -) 1M
{
    R->A = reg_inc_8(R->A);
    cpu_log(DEBUG, "Incremented $%02X", R->A);
    return true;
}
// Checked
static uint8_t reg_dec_8(uint8_t r)
{
    uint8_t result = r - 1;
    bool   is_zero = (result == 0);
    bool hc_exists = ((r & LOWER_4_MASK) == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(true, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    return result;
}
static bool dec_b(InstructionEntity *ins)      // 0x05 (Z 1 H -) 1M
{
    R->B = reg_dec_8(R->B);
    cpu_log(DEBUG, "Decremented $%02X", R->B);
    return true;
}
static bool dec_d(InstructionEntity *ins)      // 0x15 (Z 1 H -) 1M
{
    R->D = reg_dec_8(R->D);
    cpu_log(DEBUG, "Decremented $%02X", R->D);
    return true;
}
static bool dec_h(InstructionEntity *ins)      // 0x25 (Z 1 H -) 1M
{
    R->H = reg_dec_8(R->H);
    cpu_log(DEBUG, "Decremented $%02X", R->H);
    return true;
}
static bool dec_hl_mem(InstructionEntity *ins) // 0x35 (Z 1 H -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t  hl = getDR(HL_REG);
        ins->    low = read_memory(hl);
        ins->address = hl;
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, hl);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_dec_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Decremented [$%04X] - $%02X", ins->address, result);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool dec_c(InstructionEntity *ins)      // 0x0D (Z 1 H -) 1M
{
    R->C = reg_dec_8(R->C);
    cpu_log(DEBUG, "Decremented $%02X", R->C);
    return true;
}
static bool dec_e(InstructionEntity *ins)      // 0x1D (Z 1 H -) 1M
{
    R->E = reg_dec_8(R->E);
    cpu_log(DEBUG, "Decremented $%02X", R->E);
    return true;
}
static bool dec_l(InstructionEntity *ins)      // 0x2D (Z 1 H -) 1M
{
    R->L = reg_dec_8(R->L);
    cpu_log(DEBUG, "Decremented $%02X", R->L);
    return true;
}
static bool dec_a(InstructionEntity *ins)      // 0x3D (Z 1 H -) 1M
{
    R->A = reg_dec_8(R->A);
    cpu_log(DEBUG, "Decremented $%02X", R->A);
    return true;
}
// Checked
static bool rlca(InstructionEntity *ins)       // 0x07 (0 0 0 C) 1M
{
    bool c_exists = (R->A & BIT_7_MASK) != 0;
    R->A = ((R->A >> 7) | (R->A << 1));
    set_flag(false, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    cpu_log(DEBUG, "A-$%02X", R->A);
    return true;
}
static bool rla(InstructionEntity *ins)        // 0x17 (0 0 0 C) 1M
{
    bool c_exists = (R->A & BIT_7_MASK) != 0;
    uint8_t carry_in = is_flag_set(CARRY_FLAG) ? 1 : 0;
    R->A = ((R->A << 1) | carry_in);
    set_flag(false, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    cpu_log(DEBUG, "A-$%02X", R->A);
    return true;
}
static bool rrca(InstructionEntity *ins)       // 0x0F (0 0 0 C) 1M
{
    bool c_exists = (R->A & BIT_0_MASK) != 0;
    R->A = ((R->A << 7) | (R->A >> 1));
    set_flag(false, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    cpu_log(DEBUG, "A-$%02X", R->A);
    return true;
}
static bool rra(InstructionEntity *ins)        // 0x1F (0 0 0 C) 1M
{
    bool c_exists = (R->A & BIT_0_MASK) != 0;
    uint8_t carry_in = is_flag_set(CARRY_FLAG) ? 1 : 0;
    R->A = ((carry_in << 7) | (R->A >> 1));
    set_flag(false, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    cpu_log(DEBUG, "A-$%02X", R->A);
    return true;
}
// Checked
static uint16_t reg_add_16(uint16_t dest, uint16_t source)
{
    bool hc_exists = ((dest & LOWER_12_MASK) + (source & LOWER_12_MASK)) > LOWER_12_MASK;
    bool  c_exists = (dest + source) > MAX_INT_16;
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    uint16_t result = dest + source;
    return result;
}
static bool reg_add_16_handler(InstructionEntity *ins, DualRegister source)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t       hl = getDR(HL_REG);
        uint16_t  operand = getDR(source);
        uint16_t   result = reg_add_16(hl, operand);
        setDR(HL_REG, result);
        cpu_log(DEBUG, "Result - $%04X", result);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool add_hl_bc(InstructionEntity *ins)  // 0x09 (- 0 H C) 2M
{
    return reg_add_16_handler(ins, BC_REG);
}
static bool add_hl_de(InstructionEntity *ins)  // 0x19 (- 0 H C) 2M
{
    return reg_add_16_handler(ins, DE_REG);
}
static bool add_hl_hl(InstructionEntity *ins)  // 0x29 (- 0 H C) 2M
{
    return reg_add_16_handler(ins, HL_REG);
}
static bool add_hl_sp(InstructionEntity *ins)  // 0x39 (- 0 H C) 2M
{
    return reg_add_16_handler(ins, SP_REG);
}
// Checked
static uint8_t reg_add_8(uint8_t dest, uint8_t source)
{ // 0x8X
    uint8_t  result = dest + source;
    bool    is_zero = (result == 0);
    bool  hc_exists = ((dest & LOWER_4_MASK) + (source & LOWER_4_MASK)) > LOWER_4_MASK;
    bool   c_exists = (((uint16_t) dest) + ((uint16_t) source)) > LOWER_BYTE_MASK;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return (uint8_t) result;
}
static bool add_a_b(InstructionEntity *ins)    // 0x80 (Z 0 H C) 1M
{
    R->A = reg_add_8(R->A, R->B);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool add_a_c(InstructionEntity *ins)    // 0x81 (Z 0 H C) 1M
{
    R->A = reg_add_8(R->A, R->C);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool add_a_d(InstructionEntity *ins)    // 0x82 (Z 0 H C) 1M
{
    R->A = reg_add_8(R->A, R->D);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool add_a_e(InstructionEntity *ins)    // 0x83 (Z 0 H C) 1M
{
    R->A = reg_add_8(R->A, R->E);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool add_a_h(InstructionEntity *ins)    // 0x84 (Z 0 H C) 1M
{
    R->A = reg_add_8(R->A, R->H);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool add_a_l(InstructionEntity *ins)    // 0x85 (Z 0 H C) 1M
{
    R->A = reg_add_8(R->A, R->L);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool add_a_hl(InstructionEntity *ins)   // 0x86 (Z 0 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        uint8_t prev_a = R->A;
        R->A = reg_add_8(R->A, ins->low);
        cpu_log(DEBUG, "$%02X ADD $%02X = %02X", prev_a, ins->low, R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool add_a_a(InstructionEntity *ins)    // 0x87 (Z 0 H C) 1M
{
    R->A = reg_add_8(R->A, R->A);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool add_a_n(InstructionEntity *ins)    // 0xC6 (Z 0 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = reg_add_8(R->A, ins->low); 
        cpu_log(DEBUG, "Result - $%02X", R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static uint8_t reg_adc_8(uint8_t dest, uint8_t source)
{ // 0x8X
    uint8_t  carry = is_flag_set(CARRY_FLAG) ? 1 : 0;
    uint8_t result = dest + source + carry;
    bool is_zero   = (result  == 0);
    bool hc_exists = ((dest & LOWER_4_MASK) + (source & LOWER_4_MASK) + carry) > LOWER_4_MASK;
    bool c_exists  = (((uint16_t) dest) + ((uint16_t) source) + ((uint16_t) carry)) > LOWER_BYTE_MASK;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return (uint8_t) result;
}
static bool adc_a_b(InstructionEntity *ins)    // 0x88 (Z 0 H C) 1M
{
    R->A = reg_adc_8(R->A, R->B);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool adc_a_c(InstructionEntity *ins)    // 0x89 (Z 0 H C) 1M
{
    R->A = reg_adc_8(R->A, R->C);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool adc_a_d(InstructionEntity *ins)    // 0x8A (Z 0 H C) 1M
{
    R->A = reg_adc_8(R->A, R->D);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool adc_a_e(InstructionEntity *ins)    // 0x8B (Z 0 H C) 1M
{
    R->A = reg_adc_8(R->A, R->E);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool adc_a_h(InstructionEntity *ins)    // 0x8C (Z 0 H C) 1M
{
    R->A = reg_adc_8(R->A, R->H);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool adc_a_l(InstructionEntity *ins)    // 0x8D (Z 0 H C) 1M
{
    R->A = reg_adc_8(R->A, R->L);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool adc_a_hl(InstructionEntity *ins)   // 0x8E (Z 0 H C) 2M
{ 
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        uint8_t prev_a = R->A;
        R->A = reg_adc_8(R->A, ins->low);
        cpu_log(DEBUG, "$%02X ADC $%02X = %02X", prev_a, ins->low, R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool adc_a_a(InstructionEntity *ins)    // 0x8F (Z 0 H C) 1M
{
    R->A = reg_adc_8(R->A, R->A);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool adc_a_n(InstructionEntity *ins)    // 0xCE (Z 0 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = reg_adc_8(R->A, ins->low);
        cpu_log(DEBUG, "Result - $%02X", R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static uint8_t reg_sub_8(uint8_t dest, uint8_t source)
{
    uint8_t result = (dest - source);
    bool    is_zero = (result == 0);
    bool  hc_exists = ((dest & LOWER_4_MASK) < (source & LOWER_4_MASK));
    bool   c_exists = dest < source;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(true, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return (uint8_t) result;
}
static bool sub_a_b(InstructionEntity *ins)    // 0x90 (Z 1 H C) 1M
{
    R->A = reg_sub_8(R->A, R->B);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sub_a_c(InstructionEntity *ins)    // 0x91 (Z 1 H C) 1M
{
    R->A = reg_sub_8(R->A, R->C);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sub_a_d(InstructionEntity *ins)    // 0x92 (Z 1 H C) 1M
{
    R->A = reg_sub_8(R->A, R->D);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sub_a_e(InstructionEntity *ins)    // 0x93 (Z 1 H C) 1M
{
    R->A = reg_sub_8(R->A, R->E);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sub_a_h(InstructionEntity *ins)    // 0x94 (Z 1 H C) 1M
{
    R->A = reg_sub_8(R->A, R->H);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sub_a_l(InstructionEntity *ins)    // 0x95 (Z 1 H C) 1M
{
    R->A = reg_sub_8(R->A, R->L);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sub_a_hl(InstructionEntity *ins)   // 0x96 (Z 1 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        uint8_t prev_a = R->A;
        R->A = reg_sub_8(R->A, ins->low);
        cpu_log(DEBUG, "$%02X SUB $%02X = %02X", prev_a, ins->low, R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool sub_a_a(InstructionEntity *ins)    // 0x97 (Z 1 H C) 1M
{
    R->A = reg_sub_8(R->A, R->A);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sub_a_n(InstructionEntity *ins)    // 0xD6 (Z 1 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = reg_sub_8(R->A, ins->low);
        cpu_log(DEBUG, "Result - $%02X", R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static uint8_t reg_sbc_8(uint8_t dest, uint8_t source)
{
    uint8_t   carry = is_flag_set(CARRY_FLAG) ? 1 : 0;
    uint8_t  result = (dest - source - carry);
    bool    is_zero = (result == 0);
    bool  hc_exists = ((dest & LOWER_4_MASK) < ((source & LOWER_4_MASK) + carry));
    bool   c_exists = dest < (source + carry);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(true, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return (uint8_t) result;
}
static bool sbc_a_b(InstructionEntity *ins)    // 0x98 (Z 1 H C) 1M
{
    R->A = reg_sbc_8(R->A, R->B);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sbc_a_c(InstructionEntity *ins)    // 0x99 (Z 1 H C) 1M
{
    R->A = reg_sbc_8(R->A, R->C);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sbc_a_d(InstructionEntity *ins)    // 0x9A (Z 1 H C) 1M
{
    R->A = reg_sbc_8(R->A, R->D);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sbc_a_e(InstructionEntity *ins)    // 0x9B (Z 1 H C) 1M
{
    R->A = reg_sbc_8(R->A, R->E);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sbc_a_h(InstructionEntity *ins)    // 0x9C (Z 1 H C) 1M
{
    R->A = reg_sbc_8(R->A, R->H);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sbc_a_l(InstructionEntity *ins)    // 0x9D (Z 1 H C) 1M
{
    R->A = reg_sbc_8(R->A, R->L);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sbc_a_hl(InstructionEntity *ins)   // 0x9E (Z 1 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        uint8_t prev_a = R->A;
        R->A = reg_sbc_8(R->A, ins->low);
        cpu_log(DEBUG, "$%02X SBC $%02X = %02X", prev_a, ins->low, R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool sbc_a_a(InstructionEntity *ins)    // 0x9F (Z 1 H C) 1M
{
    R->A = reg_sbc_8(R->A, R->A);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool sbc_a_n(InstructionEntity *ins)    // 0xDE (Z 1 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = reg_sbc_8(R->A, ins->low);
        cpu_log(DEBUG, "Result - $%02X", R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static uint8_t reg_and_8(uint8_t dest, uint8_t source)
{
    uint8_t result = dest & source;
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(true, HALF_CARRY_FLAG);
    set_flag(false, CARRY_FLAG);
    return result;
}
static bool and_a_b(InstructionEntity *ins)    // 0xA0 (Z 0 1 0) 1M
{
    R->A = reg_and_8(R->A, R->B);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool and_a_c(InstructionEntity *ins)    // 0xA1 (Z 0 1 0) 1M
{
    R->A = reg_and_8(R->A, R->C);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool and_a_d(InstructionEntity *ins)    // 0xA2 (Z 0 1 0) 1M
{
    R->A = reg_and_8(R->A, R->D);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool and_a_e(InstructionEntity *ins)    // 0xA3 (Z 0 1 0) 1M
{
    R->A = reg_and_8(R->A, R->E);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true; 
}
static bool and_a_h(InstructionEntity *ins)    // 0xA4 (Z 0 1 0) 1M
{
    R->A = reg_and_8(R->A, R->H);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true; 
}
static bool and_a_l(InstructionEntity *ins)    // 0xA5 (Z 0 1 0) 1M
{
    R->A = reg_and_8(R->A, R->L);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool and_a_hl(InstructionEntity *ins)   // 0xA6 (Z 0 1 0) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        uint8_t prev_a = R->A;
        R->A = reg_and_8(R->A, ins->low);
        cpu_log(DEBUG, "$%02X AND $%02X = %02X", prev_a, ins->low, R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool and_a_a(InstructionEntity *ins)    // 0xA7 (Z 0 1 0) 1M
{
    R->A = reg_and_8(R->A, R->A);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true; 
}
static bool and_a_n(InstructionEntity *ins)    // 0xE6 (Z 0 1 0) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = reg_and_8(R->A, ins->low);
        cpu_log(DEBUG, "Result - $%02X", R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static uint8_t reg_xor_8(uint8_t dest, uint8_t source)
{
    uint8_t result = dest ^ source;
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(false, CARRY_FLAG);
    return result;
}
static bool xor_a_b(InstructionEntity *ins)    // 0xA8 (Z 0 0 0) 1M
{
    R->A = reg_xor_8(R->A, R->B);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool xor_a_c(InstructionEntity *ins)    // 0xA9 (Z 0 0 0) 1M
{
    R->A = reg_xor_8(R->A, R->C);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool xor_a_d(InstructionEntity *ins)    // 0xAA (Z 0 0 0) 1M
{
    R->A = reg_xor_8(R->A, R->D);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool xor_a_e(InstructionEntity *ins)    // 0xAB (Z 0 0 0) 1M
{
    R->A = reg_xor_8(R->A, R->E);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool xor_a_h(InstructionEntity *ins)    // 0xAC (Z 0 0 0) 1M
{
    R->A = reg_xor_8(R->A, R->H);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool xor_a_l(InstructionEntity *ins)    // 0xAD (Z 0 0 0) 1M
{
    R->A = reg_xor_8(R->A, R->L);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool xor_a_hl(InstructionEntity *ins)   // 0xAE (Z 0 0 0) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        uint8_t prev_a = R->A;
        R->A = reg_xor_8(R->A, ins->low);
        cpu_log(DEBUG, "$%02X XOR $%02X = %02X", prev_a, ins->low, R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool xor_a_a(InstructionEntity *ins)    // 0xAF (Z 0 0 0) 1M
{
    R->A = reg_xor_8(R->A, R->A);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool xor_a_n(InstructionEntity *ins)    // 0xEE (Z 0 0 0) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = reg_xor_8(R->A, ins->low);
        cpu_log(DEBUG, "Result - $%02X", R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static uint8_t reg_or_8(uint8_t dest, uint8_t source)
{
    uint8_t result = dest | source;
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(false, CARRY_FLAG);
    return result;
}
static bool or_a_b(InstructionEntity *ins)     // 0xB0 (Z 0 0 0) 1M
{
    R->A = reg_or_8(R->A, R->B);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool or_a_c(InstructionEntity *ins)     // 0xB1 (Z 0 0 0) 1M
{
    R->A = reg_or_8(R->A, R->C);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool or_a_d(InstructionEntity *ins)     // 0xB2 (Z 0 0 0) 1M
{
    R->A = reg_or_8(R->A, R->D);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool or_a_e(InstructionEntity *ins)     // 0xB3 (Z 0 0 0) 1M
{
    R->A = reg_or_8(R->A, R->E);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool or_a_h(InstructionEntity *ins)     // 0xB4 (Z 0 0 0) 1M
{
    R->A = reg_or_8(R->A, R->H);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool or_a_l(InstructionEntity *ins)     // 0xB5 (Z 0 0 0) 1M
{
    R->A = reg_or_8(R->A, R->L);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool or_a_hl(InstructionEntity *ins)    // 0xB6 (Z 0 0 0) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        uint8_t prev_a = R->A;
        R->A = reg_or_8(R->A, ins->low);
        cpu_log(DEBUG, "$%02X OR $%02X = %02X", prev_a, ins->low, R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool or_a_a(InstructionEntity *ins)     // 0xB7 (Z 0 0 0) 1M
{
    R->A = reg_or_8(R->A, R->A);
    cpu_log(DEBUG, "Result - $%02X", R->A);
    return true;
}
static bool or_a_n(InstructionEntity *ins)     // 0xF6 (Z 0 0 0) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = reg_or_8(R->A, ins->low);
        cpu_log(DEBUG, "Result - $%02X", R->A);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static void reg_cp_8(uint8_t dest, uint8_t source)
{
    bool    is_zero = (dest == source); 
    bool  hc_exists = ((dest & LOWER_4_MASK) < (source & LOWER_4_MASK));
    bool   c_exists = dest < source;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(true, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
static bool cp_a_b(InstructionEntity *ins)     // 0xB8 (Z 1 H C) 1M
{
    reg_cp_8(R->A, R->B);
    cpu_log(DEBUG, "%02X < %02X = %d", R->A, R->B, is_flag_set(CARRY_FLAG));
    return true;
}
static bool cp_a_c(InstructionEntity *ins)     // 0xB9 (Z 1 H C) 1M
{
    reg_cp_8(R->A, R->C);
    cpu_log(DEBUG, "%02X < %02X = %d", R->A, R->C, is_flag_set(CARRY_FLAG));
    return true;
}
static bool cp_a_d(InstructionEntity *ins)     // 0xBA (Z 1 H C) 1M
{
    reg_cp_8(R->A, R->D);
    cpu_log(DEBUG, "%02X < %02X = %d", R->A, R->D, is_flag_set(CARRY_FLAG));
    return true;
}
static bool cp_a_e(InstructionEntity *ins)     // 0xBB (Z 1 H C) 1M
{
    reg_cp_8(R->A, R->E);
    cpu_log(DEBUG, "%02X < %02X = %d", R->A, R->E, is_flag_set(CARRY_FLAG));
    return true;
}
static bool cp_a_h(InstructionEntity *ins)     // 0xBC (Z 1 H C) 1M
{
    reg_cp_8(R->A, R->H);
    cpu_log(DEBUG, "%02X < %02X = %d", R->A, R->H, is_flag_set(CARRY_FLAG));
    return true;
}
static bool cp_a_l(InstructionEntity *ins)     // 0xBD (Z 1 H C) 1M
{
    reg_cp_8(R->A, R->L);
    cpu_log(DEBUG, "%02X < %02X = %d", R->A, R->L, is_flag_set(CARRY_FLAG));
    return true;
}
static bool cp_a_hl(InstructionEntity *ins)    // 0xBE (Z 1 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        uint16_t hl = getDR(HL_REG);
        ins->low = read_memory(hl);
        reg_cp_8(R->A, ins->low);
        cpu_log(DEBUG, "%02X < %02X %d", R->A, ins->low, is_flag_set(CARRY_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool cp_a_a(InstructionEntity *ins)     // 0xBF (Z 1 H C) 1M
{
    reg_cp_8(R->A, R->A);
    cpu_log(DEBUG, "%02X < %02X = %d", R->A, R->A, is_flag_set(CARRY_FLAG));
    return true;
}
static bool cp_a_n(InstructionEntity *ins)     // 0xFE (Z 1 H C) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        reg_cp_8(R->A, ins->low);
        cpu_log(DEBUG, "%02X < %02X %d", R->A, ins->low, is_flag_set(CARRY_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool return_handler(InstructionEntity *ins, bool returning)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        if (!returning)
        {
            cpu_log(DEBUG, "Condition not met, stopping early.");
        }
        return !returning;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->low = pop_stack();
        cpu_log(DEBUG, "Popped $%02X", ins->low);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        ins->high = pop_stack();
        cpu_log(DEBUG, "Popped $%02X", ins->high);
        return false;
    }

    if (ins->duration == 5) // Fifth Cycle
    {
        ins->address = form_address(ins);
        R->PC = ins->address;
        cpu_log(DEBUG, "Address - [$%04X]", R->PC);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ret_nz(InstructionEntity *ins)     // 0xC0 (- - - -) 5M
{
    return return_handler(ins, !is_flag_set(ZERO_FLAG));
}
static bool ret_nc(InstructionEntity *ins)     // 0xD0 (- - - -) 5M
{
    return return_handler(ins, !is_flag_set(CARRY_FLAG));
}
static bool ret_c(InstructionEntity *ins)      // 0xD8 (- - - -) 5M
{
    return return_handler(ins, is_flag_set(CARRY_FLAG));
}
static bool ret_z(InstructionEntity *ins)      // 0xC8 (- - - -) 5M
{
    return return_handler(ins, is_flag_set(ZERO_FLAG));
}
static bool ret(InstructionEntity *ins)        // 0xC9 (- - - -) 4M
{
    if (ins->duration == 2) ins->duration += 1; // Skips check cycle
    return return_handler(ins, true);
}
static bool reti(InstructionEntity *ins)       // 0xD9 (- - - -) 4M
{
    if (ins->duration == 2) ins->duration += 1; // Skips check cycle
    bool result = return_handler(ins, true);
    if (ins->duration == 4) schedule_ime(iee);
    return result;
}
// Checked
static bool rst_handler(InstructionEntity *ins, uint16_t rst_vector)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->high = (R->PC >> BYTE) & LOWER_BYTE_MASK;
        push_stack(ins->high);
        cpu_log(DEBUG, "Pushed $%02X", ins->high);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->low = R->PC & LOWER_BYTE_MASK;
        push_stack(ins->low);
        cpu_log(DEBUG, "Pushed $%02X", ins->low);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        R->PC = rst_vector;
        cpu_log(DEBUG, "Subroutine $%02X", rst_vector);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool rst_00(InstructionEntity *ins)     // 0xC7 (- - - -) 4M
{
    return rst_handler(ins, 0x00);
}
static bool rst_10(InstructionEntity *ins)     // 0xD7 (- - - -) 4M
{
    return rst_handler(ins, 0x10);
}
static bool rst_20(InstructionEntity *ins)     // 0xE7 (- - - -) 4M
{
    return rst_handler(ins, 0x20);
}
static bool rst_30(InstructionEntity *ins)     // 0xF7 (- - - -) 4M
{
    return rst_handler(ins, 0x30);
}
static bool rst_08(InstructionEntity *ins)     // 0xCF (- - - -) 4M
{
    return rst_handler(ins, 0x08);
}
static bool rst_18(InstructionEntity *ins)     // 0xDF (- - - -) 4M
{ 
    return rst_handler(ins, 0x18);
}
static bool rst_28(InstructionEntity *ins)     // 0xEF (- - - -) 4M
{
    return rst_handler(ins, 0x28);
}
static bool rst_38(InstructionEntity *ins)     // 0xFF (- - - -) 4M
{
    return rst_handler(ins, 0x38);
}
// Checked
static bool call_handler(InstructionEntity *ins, bool calling)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->high = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->high);
        if (!calling)
        {
            cpu_log(DEBUG, "Condition not met, stopping early.");
            return true;
        }
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        uint8_t pc_high = (R->PC >> BYTE) & LOWER_BYTE_MASK;
        push_stack(pc_high);
        cpu_log(DEBUG, "Pushed $%02X", pc_high);
        return false;
    }

    if (ins->duration == 5) // Fifth Cycle
    {
        uint8_t pc_low = R->PC & LOWER_BYTE_MASK;
        push_stack(pc_low);
        cpu_log(DEBUG, "Pushed $%02X", pc_low);
        return false;
    }

    if (ins->duration == 6) // Sixth Cycle
    {   
        ins->address = form_address(ins);
        R->PC = ins->address;
        cpu_log(DEBUG, "Address [$%04X]", ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool call_nz_nn(InstructionEntity *ins) // 0xC4 (- - - -) 6M
{
    return call_handler(ins, !is_flag_set(ZERO_FLAG));
}
static bool call_nc_nn(InstructionEntity *ins) // 0xD4 (- - - -) 6M
{
    return call_handler(ins, !is_flag_set(CARRY_FLAG));
}
static bool call_z_nn(InstructionEntity *ins)  // 0xCC (- - - -) 6M
{
    return call_handler(ins, is_flag_set(ZERO_FLAG));
}
static bool call_c_nn(InstructionEntity *ins)  // 0xDC (- - - -) 6M
{
    return call_handler(ins, is_flag_set(CARRY_FLAG));
}
static bool call_nn(InstructionEntity *ins)    // 0xCD (- - - -) 6M
{
    return call_handler(ins, true);
}
// Checked
static bool jump_relative_handler(InstructionEntity *ins, bool jumping)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        if (!jumping)
        {
            cpu_log(DEBUG, "Condition not met, stopping early.");
            return true;
        }
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        int8_t offset = (int8_t) ins->low;
        uint16_t old_pc = R->PC;
        R->PC += offset;
        cpu_log(DEBUG, "$%04X to $%04X (offset %d)", old_pc, R->PC, offset); 
        return true;      
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool jr_n(InstructionEntity *ins)       // 0x18 (- - - -) 3M
{
    return jump_relative_handler(ins, true);
}
static bool jr_z_n(InstructionEntity *ins)     // 0x28 (- - - -) 3M
{
    return jump_relative_handler(ins, is_flag_set(ZERO_FLAG));
}
static bool jr_c_n(InstructionEntity *ins)     // 0x38 (- - - -) 3M
{
    return jump_relative_handler(ins, is_flag_set(CARRY_FLAG));
}
static bool jr_nz_n(InstructionEntity *ins)    // 0x20 (- - - -) 3M
{
    return jump_relative_handler(ins, !is_flag_set(ZERO_FLAG));
}
static bool jr_nc_n(InstructionEntity *ins)    // 0x30 (- - - -) 3M
{
    return jump_relative_handler(ins, !is_flag_set(CARRY_FLAG));
}
// Checked
static bool jump_position_handler(InstructionEntity *ins, bool jumping)
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->high = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->high);
        if (!jumping)
        {
            cpu_log(DEBUG, "Condition not met, stopping early.");
            return true;
        }
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        ins->address = form_address(ins);
        uint16_t old_pc = R->PC;
        R->PC = ins->address;
        cpu_log(DEBUG, "$%04X to $%04X", old_pc, R->PC);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool jp_nz_nn(InstructionEntity *ins)   // 0xC2 (- - - -) 4M
{
    return jump_position_handler(ins, !is_flag_set(ZERO_FLAG));
}
static bool jp_nc_nn(InstructionEntity *ins)   // 0xD2 (- - - -) 4M
{
    return jump_position_handler(ins, !is_flag_set(CARRY_FLAG));
}
static bool jp_nn(InstructionEntity *ins)      // 0xC3 (- - - -) 4M
{
    return jump_position_handler(ins, true);
}
static bool jp_z_nn(InstructionEntity *ins)    // 0xCA (- - - -) 4M
{
    return jump_position_handler(ins, is_flag_set(ZERO_FLAG));
}
static bool jp_c_nn(InstructionEntity *ins)    // 0xDA (- - - -) 4M
{
    return jump_position_handler(ins, is_flag_set(CARRY_FLAG));
}
static bool jp_hl(InstructionEntity *ins)      // 0xE9 (- - - -) 1M
{
    R->PC = getDR(HL_REG);
    cpu_log(DEBUG, "Address $%04X", R->PC);
    return true;
}
// Checked
static bool daa(InstructionEntity *ins)        // 0x27 (Z - 0 C) 1M
{
    uint8_t correction = 0;
    bool carry = is_flag_set(CARRY_FLAG);

    if (!is_flag_set(SUBTRACT_FLAG)) 
    {
        if (is_flag_set(HALF_CARRY_FLAG) || ((R->A & 0x0F) > 9)) correction |= 0x06;
        if (carry || R->A > 0x99) 
        {
            correction |= 0x60;
            set_flag(true, CARRY_FLAG);
        } 
        else 
        {
            set_flag(false, CARRY_FLAG);
        }
        R->A += correction;
    } 
    else 
    {
        if (is_flag_set(HALF_CARRY_FLAG)) correction |= 0x06;
        if (carry)                        correction |= 0x60;
        R->A -= correction;
    }

    set_flag((R->A == 0), ZERO_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    cpu_log(DEBUG, "A=$%02X", R->A);
    return true;
}

static bool cpl(InstructionEntity *ins)        // 0x2F (- 1 1 -) 1M
{
    R->A = ~R->A;
    set_flag(true, SUBTRACT_FLAG);
    set_flag(true, HALF_CARRY_FLAG);
    cpu_log(DEBUG, "$%02X", R->A);
    return true;
}
static bool scf(InstructionEntity *ins)        // 0x07 (- 0 0 1) 1M
{
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(true, CARRY_FLAG);
    cpu_log(DEBUG, "...");
    return true;
}
static bool ccf(InstructionEntity *ins)        // 0x3F (- 0 0 C) 1M
{
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(!is_flag_set(CARRY_FLAG), CARRY_FLAG);
    cpu_log(DEBUG, "...");
    return true;
}
// Checked
static bool ldh_n_a(InstructionEntity *ins)    // 0xE0 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->    low = fetch();
        ins->address = (0xFF00 | ins->low);
        cpu_log(DEBUG, "Fetched $%02X, Address [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        write_memory(ins->address, R->A);
        cpu_log(DEBUG, "Wrote $%02X into [$%04X]", R->A, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ldh_a_n(InstructionEntity *ins)    // 0xF0 (- - - -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->    low = fetch();
        ins->address = (0xFF00 | ins->low);
        cpu_log(DEBUG, "Fetched $%02X, Address [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        R->A = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", R->A, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ldh_c_a(InstructionEntity *ins)    // 0xE1 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->address = (0xFF00 | R->C);
        cpu_log(DEBUG, "Address [$%04X]", ins->address);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        write_memory(ins->address, R->A);
        cpu_log(DEBUG, "Wrote $%02X into [$%04X]", R->A, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ldh_a_c(InstructionEntity *ins)    // 0xF1 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        ins->address = (0xFF00 | R->C);
        cpu_log(DEBUG, "Address [$%04X]", ins->address);
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->A = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", R->A, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool ld_nn_a(InstructionEntity *ins)    // 0xEA (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->high = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->high);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        ins->address = form_address(ins);
        write_memory(ins->address, R->A);
        cpu_log(DEBUG, "Wrote $%02X into [$%04X]", R->A, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_a_nn(InstructionEntity *ins)    // 0xFA (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        ins->high = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->high);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        ins->address = form_address(ins);
        R->A = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", R->A, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_hl_sp_n(InstructionEntity *ins) // 0xF8 (0 0 H C) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        int8_t        n = (int8_t) ins->low;
        uint16_t result = R->SP + n;
        bool hc_exists = ((R->SP & LOWER_4_MASK) + (((uint8_t) n) & LOWER_4_MASK)) > LOWER_4_MASK;
        bool c_exists  = ((R->SP & 0xFF) + (((uint8_t) n) & 0xFF)) > 0xFF;
        set_flag(false, ZERO_FLAG);
        set_flag(false, SUBTRACT_FLAG);
        set_flag(hc_exists, HALF_CARRY_FLAG);
        set_flag(c_exists, CARRY_FLAG);
        setDR(HL_REG, result);
        cpu_log(DEBUG, "HL - $%04X", getDR(HL_REG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool ld_sp_hl(InstructionEntity *ins)   // 0xF9 (- - - -) 2M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        R->SP = getDR(HL_REG);
        cpu_log(DEBUG, "$%02X", R->SP);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool add_sp_n(InstructionEntity *ins)   // 0xE8 (0 0 H C) 4M
{
    if (ins->duration <= 2) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 3) // Second Cycle
    {
        ins->low = fetch();
        cpu_log(DEBUG, "Fetched $%02X", ins->low);
        return false;
    }

    if (ins->duration == 4) // Third Cycle
    {
        int8_t        n = (int8_t) ins->low;
        uint16_t    sum = (R->SP + n);
        bool  hc_exists = ((R->SP & LOWER_4_MASK) + (((uint8_t) n) & LOWER_4_MASK)) > LOWER_4_MASK;
        bool  c_exists  = ((R->SP & 0xFF) + (((uint8_t) n) & 0xFF)) > 0xFF;
        set_flag(false, ZERO_FLAG);
        set_flag(false, SUBTRACT_FLAG);
        set_flag(hc_exists, HALF_CARRY_FLAG);
        set_flag(c_exists, CARRY_FLAG);
        R->SP = sum;
        cpu_log(DEBUG, "$%04X", R->SP);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
static bool di(InstructionEntity *ins)         // 0xF3 (- - - -) 1M
{
    cpu->   ime = false;
    iee->active = false;
    cpu_log(DEBUG, "Disable Interrupt Request");
    return true;
}
static bool ei(InstructionEntity *ins)         // 0xFB (- - - -) 1M
{
    schedule_ime(iee);
    cpu_log(DEBUG, "IEE scheduled, expect after next instruction.");
    return true;
}
static bool cb_prefix(InstructionEntity *ins)  // 0xCB (- - - -) 1m
{
    cpu_log(DEBUG, "...");
    cb_prefixed = true;
    return true;
}
// Checked
static bool vram_print(InstructionEntity *ins) // 0xXX (- - - -) 1M
{
    uint8_t lower  = fetch();
    uint8_t upper  = fetch();
    uint16_t start = (upper << BYTE) | lower;
    lower          = fetch();
    upper          = fetch();
    uint16_t end   = (upper << BYTE) | lower;
    bool bank      = (bool) fetch();
    print_vram(start, end, bank);
    return 1; 
}
static bool int_exec(InstructionEntity *ins)   // 0xXX (- - - -) 5M
{
    if (ins->duration <= 2) // First & Second Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t pc_high = (R->PC >> BYTE) & LOWER_BYTE_MASK;
        push_stack(pc_high);
        cpu_log(DEBUG, "Pushed $%02X", pc_high);
        return false;
    }

    if (ins->duration == 4) // Fourth Cycle
    {
        uint8_t pc_low = R->PC & LOWER_BYTE_MASK;
        push_stack(pc_low);
        cpu_log(DEBUG, "Pushed $%02X", pc_low);
        return false;
    }

    if (ins->duration == 5) // Fifth Cycle
    {
        R->PC = ins->address;    // Set when serviciing
        write_ifr(*R->IFR & ~ins->low);  // Confirm servicing
        cpu_log(DEBUG, "Address $%04X", ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
// Checked
const OpcodeHandler opcode_table[256] = 
{
    /*              ROW 1               */
    [0x00] = nop,   
    [0x01] = ld_bc_nn,
    [0x02] = ld_bc_a,
    [0x03] = inc_bc,
    [0x04] = inc_b,
    [0x05] = dec_b,
    [0x06] = ld_b_n,
    [0x07] = rlca,
    [0x08] = ld_nn_sp,
    [0x09] = add_hl_bc,
    [0x0A] = ld_a_bc,
    [0x0B] = dec_bc,
    [0x0C] = inc_c,
    [0x0D] = dec_c,
    [0x0E] = ld_c_n,
    [0x0F] = rrca,
    /*              ROW 2               */
    [0x10] = stop,
    [0x11] = ld_de_nn,
    [0x12] = ld_de_a,
    [0x13] = inc_de,
    [0x14] = inc_d,
    [0x15] = dec_d,
    [0x16] = ld_d_n,
    [0x17] = rla,
    [0x18] = jr_n,
    [0x19] = add_hl_de,
    [0x1A] = ld_a_de,
    [0x1B] = dec_de,
    [0x1C] = inc_e,
    [0x1D] = dec_e,
    [0x1E] = ld_e_n,
    [0x1F] = rra,
    /*              ROW 3               */
    [0x20] = jr_nz_n,
    [0x21] = ld_hl_nn,
    [0x22] = ld_hli_a,
    [0x23] = inc_hl,
    [0x24] = inc_h,
    [0x25] = dec_h,
    [0x26] = ld_h_n,
    [0x27] = daa,
    [0x28] = jr_z_n,
    [0x29] = add_hl_hl,
    [0x2A] = ld_a_hli,
    [0x2B] = dec_hl,
    [0x2C] = inc_l,
    [0x2D] = dec_l,
    [0x2E] = ld_l_n,
    [0x2F] = cpl,
    /*              ROW 4               */
    [0x30] = jr_nc_n,
    [0x31] = ld_sp_nn,
    [0x32] = ld_hld_a,
    [0x33] = inc_sp,
    [0x34] = inc_hl_mem,
    [0x35] = dec_hl_mem,
    [0x36] = ld_hl_n,
    [0x37] = scf,
    [0x38] = jr_c_n,
    [0x39] = add_hl_sp,
    [0x3A] = ld_a_hld,
    [0x3B] = dec_sp,
    [0x3C] = inc_a,
    [0x3D] = dec_a,
    [0x3E] = ld_a_n,
    [0x3F] = ccf,
    /*              ROW 5               */
    [0x40] = nop,   
    [0x41] = ld_b_c,
    [0x42] = ld_b_d,
    [0x43] = ld_b_e,
    [0x44] = ld_b_h,
    [0x45] = ld_b_l,
    [0x46] = ld_b_hl,
    [0x47] = ld_b_a,
    [0x48] = ld_c_b,
    [0x49] = nop,
    [0x4A] = ld_c_d,
    [0x4B] = ld_c_e,
    [0x4C] = ld_c_h,
    [0x4D] = ld_c_l,
    [0x4E] = ld_c_hl,
    [0x4F] = ld_c_a,
    /*              ROW 6               */
    [0x50] = ld_d_b,
    [0x51] = ld_d_c,
    [0x52] = nop,
    [0x53] = ld_d_e,
    [0x54] = ld_d_h,
    [0x55] = ld_d_l,
    [0x56] = ld_d_hl,
    [0x57] = ld_d_a,
    [0x58] = ld_e_b,
    [0x59] = ld_e_c,
    [0x5A] = ld_e_d,
    [0x5B] = nop,
    [0x5C] = ld_e_h,
    [0x5D] = ld_e_l,
    [0x5E] = ld_e_hl,
    [0x5F] = ld_e_a,
    /*              ROW 7               */
    [0x60] = ld_h_b,
    [0x61] = ld_h_c,
    [0x62] = ld_h_d,
    [0x63] = ld_h_e,
    [0x64] = nop,
    [0x65] = ld_h_l,
    [0x66] = ld_h_hl,
    [0x67] = ld_h_a,
    [0x68] = ld_l_b,
    [0x69] = ld_l_c,
    [0x6A] = ld_l_d,
    [0x6B] = ld_l_e,
    [0x6C] = ld_l_h,
    [0x6D] = nop,
    [0x6E] = ld_l_hl,
    [0x6F] = ld_l_a,
    /*              ROW 8               */
    [0x70] = ld_hl_b,
    [0x71] = ld_hl_c,
    [0x72] = ld_hl_d,
    [0x73] = ld_hl_e,
    [0x74] = ld_hl_h,
    [0x75] = ld_hl_l,
    [0x76] = halt,
    [0x77] = ld_hl_a,
    [0x78] = ld_a_b,
    [0x79] = ld_a_c,
    [0x7A] = ld_a_d,
    [0x7B] = ld_a_e,
    [0x7C] = ld_a_h,
    [0x7D] = ld_a_l,
    [0x7E] = ld_a_hl,
    [0x7F] = nop,
    /*              ROW 9               */
    [0x80] = add_a_b,
    [0x81] = add_a_c,
    [0x82] = add_a_d,
    [0x83] = add_a_e,
    [0x84] = add_a_h,
    [0x85] = add_a_l,
    [0x86] = add_a_hl,
    [0x87] = add_a_a,
    [0x88] = adc_a_b,
    [0x89] = adc_a_c,
    [0x8A] = adc_a_d,
    [0x8B] = adc_a_e,
    [0x8C] = adc_a_h,
    [0x8D] = adc_a_l,
    [0x8E] = adc_a_hl,
    [0x8F] = adc_a_a,
    /*              ROW 10              */
    [0x90] = sub_a_b,
    [0x91] = sub_a_c,
    [0x92] = sub_a_d,
    [0x93] = sub_a_e,
    [0x94] = sub_a_h,
    [0x95] = sub_a_l,
    [0x96] = sub_a_hl,
    [0x97] = sub_a_a,
    [0x98] = sbc_a_b,
    [0x99] = sbc_a_c,
    [0x9A] = sbc_a_d,
    [0x9B] = sbc_a_e,
    [0x9C] = sbc_a_h,
    [0x9D] = sbc_a_l,
    [0x9E] = sbc_a_hl,
    [0x9F] = sbc_a_a,
    /*              ROW 11              */
    [0xA0] = and_a_b,
    [0xA1] = and_a_c,
    [0xA2] = and_a_d,
    [0xA3] = and_a_e,
    [0xA4] = and_a_h,
    [0xA5] = and_a_l,
    [0xA6] = and_a_hl,
    [0xA7] = and_a_a,
    [0xA8] = xor_a_b,
    [0xA9] = xor_a_c,
    [0xAA] = xor_a_d,
    [0xAB] = xor_a_e,
    [0xAC] = xor_a_h,
    [0xAD] = xor_a_l,
    [0xAE] = xor_a_hl,
    [0xAF] = xor_a_a,
    /*              ROW 12              */
    [0xB0] = or_a_b,
    [0xB1] = or_a_c,
    [0xB2] = or_a_d,
    [0xB3] = or_a_e,
    [0xB4] = or_a_h,
    [0xB5] = or_a_l,
    [0xB6] = or_a_hl,
    [0xB7] = or_a_a,
    [0xB8] = cp_a_b,
    [0xB9] = cp_a_c,
    [0xBA] = cp_a_d,
    [0xBB] = cp_a_e,
    [0xBC] = cp_a_h,
    [0xBD] = cp_a_l,
    [0xBE] = cp_a_hl,
    [0xBF] = cp_a_a,
    /*              ROW 13              */
    [0xC0] = ret_nz,
    [0xC1] = pop_bc,
    [0xC2] = jp_nz_nn,
    [0xC3] = jp_nn,
    [0xC4] = call_nz_nn,
    [0xC5] = push_bc,
    [0xC6] = add_a_n,
    [0xC7] = rst_00,
    [0xC8] = ret_z,
    [0xC9] = ret,
    [0xCA] = jp_z_nn,
    [0xCB] = cb_prefix,
    [0xCC] = call_z_nn,
    [0xCD] = call_nn,
    [0xCE] = adc_a_n,
    [0xCF] = rst_08,
    /*              ROW 14              */
    [0xD0] = ret_nc,
    [0xD1] = pop_de,
    [0xD2] = jp_nc_nn,
    [0xD3] = vram_print,
    [0xD4] = call_nc_nn,
    [0xD5] = push_de,
    [0xD6] = sub_a_n,
    [0xD7] = rst_10,
    [0xD8] = ret_c,
    [0xD9] = reti,
    [0xDA] = jp_c_nn,
    [0xDB] = nop,
    [0xDC] = call_c_nn,
    [0xDD] = nop,
    [0xDE] = sbc_a_n,
    [0xDF] = rst_18,
    /*              ROW 15              */
    [0xE0] = ldh_n_a,
    [0xE1] = pop_hl,
    [0xE2] = ldh_c_a,
    [0xE3] = nop,
    [0xE4] = nop,
    [0xE5] = push_hl,
    [0xE6] = and_a_n,
    [0xE7] = rst_20,
    [0xE8] = add_sp_n,
    [0xE9] = jp_hl,
    [0xEA] = ld_nn_a,
    [0xEB] = nop,
    [0xEC] = nop,
    [0xED] = nop,
    [0xEE] = xor_a_n,
    [0xEF] = rst_28,
    /*              ROW 16              */
    [0xF0] = ldh_a_n,
    [0xF1] = pop_af,
    [0xF2] = ldh_a_c,
    [0xF3] = di,
    [0xF4] = nop,
    [0xF5] = push_af,
    [0xF6] = or_a_n,
    [0xF7] = rst_30,
    [0xF8] = ld_hl_sp_n,
    [0xF9] = ld_sp_hl,
    [0xFA] = ld_a_nn,
    [0xFB] = ei,
    [0xFC] = nop,
    [0xFD] = nop,
    [0xFE] = cp_a_n,
    [0xFF] = rst_38,
};
// Checked
static uint8_t reg_rlc_8(uint8_t r)
{
    bool  c_exists = ((r & BIT_7_MASK) != 0);
    uint8_t carry_in = c_exists ? 1 : 0;
    uint8_t   result = ((r << 1) | carry_in);
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
static bool rlc_b(InstructionEntity *ins)     // 0x00 (Z 0 0 C) 2M
{
    R->B = reg_rlc_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool rlc_c(InstructionEntity *ins)     // 0x01 (Z 0 0 C) 2M
{
    R->C = reg_rlc_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}  
static bool rlc_d(InstructionEntity *ins)     // 0x02 (Z 0 0 C) 2M
{
    R->D = reg_rlc_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}  
static bool rlc_e(InstructionEntity *ins)     // 0x03 (Z 0 0 C) 2M
{
    R->E = reg_rlc_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}  
static bool rlc_h(InstructionEntity *ins)     // 0x04 (Z 0 0 C) 2M
{
    R->H = reg_rlc_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}  
static bool rlc_l(InstructionEntity *ins)     // 0x05 (Z 0 0 C) 2M
{
    R->L = reg_rlc_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}  
static bool rlc_hl(InstructionEntity *ins)    // 0x06 (Z 0 0 C) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_rlc_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}  
static bool rlc_a(InstructionEntity *ins)     // 0x07 (Z 0 0 C) 2M
{
    R->A = reg_rlc_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}     
// Checked
static uint8_t reg_rrc_8(uint8_t r)
{
    bool  c_exists = ((r & BIT_0_MASK) != 0);
    uint8_t carry_in = c_exists ? 1 : 0;
    uint8_t result = ((carry_in << 7) | (r >> 1));
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
static bool rrc_b(InstructionEntity *ins)     // 0x08 (Z 0 0 C) 2M
{
    R->B = reg_rrc_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool rrc_c(InstructionEntity *ins)     // 0x09 (Z 0 0 C) 2M
{
    R->C = reg_rrc_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool rrc_d(InstructionEntity *ins)     // 0x0A (Z 0 0 C) 2M
{
    R->D = reg_rrc_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool rrc_e(InstructionEntity *ins)     // 0x0B (Z 0 0 C) 2M
{
    R->E = reg_rrc_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool rrc_h(InstructionEntity *ins)     // 0x0C (Z 0 0 C) 2M
{
    R->H = reg_rrc_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool rrc_l(InstructionEntity *ins)     // 0x0D (Z 0 0 C) 2M
{
    R->L = reg_rrc_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool rrc_hl(InstructionEntity *ins)    // 0x0E (Z 0 0 C) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_rrc_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool rrc_a(InstructionEntity *ins)     // 0x0F (Z 0 0 C) 2M
{
    R->A = reg_rrc_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static uint8_t reg_rl_8(uint8_t r)
{
    bool    c_exists = ((r & BIT_7_MASK) != 0);
    uint8_t carry_in = is_flag_set(CARRY_FLAG) ? 1 : 0;
    uint8_t   result = ((r << 1) | carry_in);
    bool     is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
static bool rl_b(InstructionEntity *ins)      // 0x10 (Z 0 0 C) 2M
{
    R->B = reg_rl_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool rl_c(InstructionEntity *ins)      // 0x11 (Z 0 0 C) 2M
{
    R->C = reg_rl_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool rl_d(InstructionEntity *ins)      // 0x12 (Z 0 0 C) 2M
{
    R->D = reg_rl_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool rl_e(InstructionEntity *ins)      // 0x13 (Z 0 0 C) 2M
{
    R->E = reg_rl_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool rl_h(InstructionEntity *ins)      // 0x14 (Z 0 0 C) 2M
{
    R->H = reg_rl_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool rl_l(InstructionEntity *ins)      // 0x15 (Z 0 0 C) 2M
{
    R->L = reg_rl_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool rl_hl(InstructionEntity *ins)     // 0x16 (Z 0 0 C) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_rl_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool rl_a(InstructionEntity *ins)      // 0x17 (Z 0 0 C) 2M
{
    R->A = reg_rl_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static uint8_t reg_rr_8(uint8_t r)
{
    bool    c_exists = ((r & BIT_0_MASK) != 0);
    uint8_t carry_in = is_flag_set(CARRY_FLAG) ? 1 : 0;
    uint8_t   result = ((carry_in << 7) | (r >> 1));
    bool     is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
static bool rr_b(InstructionEntity *ins)      // 0x18 (Z 0 0 C) 2M
{
    R->B = reg_rr_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool rr_c(InstructionEntity *ins)      // 0x19 (Z 0 0 C) 2M
{
    R->C = reg_rr_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool rr_d(InstructionEntity *ins)      // 0x1A (Z 0 0 C) 2M
{
    R->D = reg_rr_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool rr_e(InstructionEntity *ins)      // 0x1B (Z 0 0 C) 2M
{
    R->E = reg_rr_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool rr_h(InstructionEntity *ins)      // 0x1C (Z 0 0 C) 2M
{
    R->H = reg_rr_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool rr_l(InstructionEntity *ins)      // 0x1D (Z 0 0 C) 2M
{
    R->L = reg_rr_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool rr_hl(InstructionEntity *ins)     // 0x1E (Z 0 0 C) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_rr_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool rr_a(InstructionEntity *ins)      // 0x1F (Z 0 0 C) 2M
{
    R->A = reg_rr_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static uint8_t reg_sla_8(uint8_t r)
{
    bool  c_exists = ((r & BIT_7_MASK) != 0);
    uint8_t result = (r << 1);
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
static bool sla_b(InstructionEntity *ins)     // 0x20 (Z 0 0 C) 2M
{
    R->B = reg_sla_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool sla_c(InstructionEntity *ins)     // 0x21 (Z 0 0 C) 2M
{
    R->C = reg_sla_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool sla_d(InstructionEntity *ins)     // 0x22 (Z 0 0 C) 2M
{
    R->D = reg_sla_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool sla_e(InstructionEntity *ins)     // 0x23 (Z 0 0 C) 2M
{
    R->E = reg_sla_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool sla_h(InstructionEntity *ins)     // 0x24 (Z 0 0 C) 2M
{
    R->H = reg_sla_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool sla_l(InstructionEntity *ins)     // 0x25 (Z 0 0 C) 2M
{
    R->L = reg_sla_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool sla_hl(InstructionEntity *ins)    // 0x26 (Z 0 0 C) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_sla_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool sla_a(InstructionEntity *ins)     // 0x27 (Z 0 0 C) 2M
{
    R->A = reg_sla_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static uint8_t reg_sra_8(uint8_t r)
{
    bool  c_exists = (r & BIT_0_MASK) != 0;
    uint8_t result = (r & BIT_7_MASK) | (r >> 1);
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
static bool sra_b(InstructionEntity *ins)     // 0x28 (Z 0 0 C) 2M
{
    R->B = reg_sra_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool sra_c(InstructionEntity *ins)     // 0x29 (Z 0 0 C) 2M
{
    R->C = reg_sra_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool sra_d(InstructionEntity *ins)     // 0x2A (Z 0 0 C) 2M
{
    R->D = reg_sra_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool sra_e(InstructionEntity *ins)     // 0x2B (Z 0 0 C) 2M
{
    R->E = reg_sra_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool sra_h(InstructionEntity *ins)     // 0x2C (Z 0 0 C) 2M
{
    R->H = reg_sra_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool sra_l(InstructionEntity *ins)     // 0x2D (Z 0 0 C) 2M
{
    R->L = reg_sra_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool sra_hl(InstructionEntity *ins)    // 0x2E (Z 0 0 C) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_sra_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool sra_a(InstructionEntity *ins)     // 0x2F (Z 0 0 C) 2M
{
    R->A = reg_sra_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static uint8_t reg_swap_8(uint8_t r)
{
    uint8_t result  = ((r >> NIBBLE) | (r << NIBBLE));
    bool    is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(false, CARRY_FLAG);
    return result;
}
static bool swap_b(InstructionEntity *ins)    // 0x30 (Z 0 0 0) 2M
{
    R->B = reg_swap_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool swap_c(InstructionEntity *ins)    // 0x31 (Z 0 0 0) 2M
{
    R->C = reg_swap_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;  
}
static bool swap_d(InstructionEntity *ins)    // 0x32 (Z 0 0 0) 2M
{
    R->D = reg_swap_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool swap_e(InstructionEntity *ins)    // 0x33 (Z 0 0 0) 2M
{
    R->E = reg_swap_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool swap_h(InstructionEntity *ins)    // 0x34 (Z 0 0 0) 2M
{
    R->H = reg_swap_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool swap_l(InstructionEntity *ins)    // 0x35 (Z 0 0 0) 2M
{
    R->L = reg_swap_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool swap_hl(InstructionEntity *ins)   // 0x36 (Z 0 0 0) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_swap_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool swap_a(InstructionEntity *ins)    // 0x37 (Z 0 0 0) 2M
{
    R->A = reg_swap_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static uint8_t reg_srl_8(uint8_t r)
{
    bool  c_exists = ((r & BIT_0_MASK) != 0);
    uint8_t result = (r >> 1);
    bool   is_zero = (result == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
static bool srl_b(InstructionEntity *ins)     // 0x38 (Z 0 0 C) 2M
{
    R->B = reg_srl_8(R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool srl_c(InstructionEntity *ins)     // 0x39 (Z 0 0 C) 2M
{
    R->C = reg_srl_8(R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool srl_d(InstructionEntity *ins)     // 0x3A (Z 0 0 C) 2M
{
    R->D = reg_srl_8(R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool srl_e(InstructionEntity *ins)     // 0x3B (Z 0 0 C) 2M
{
    R->E = reg_srl_8(R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool srl_h(InstructionEntity *ins)     // 0x3C (Z 0 0 C) 2M
{
    R->H = reg_srl_8(R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool srl_l(InstructionEntity *ins)     // 0x3D (Z 0 0 C) 2M
{
    R->L = reg_srl_8(R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool srl_hl(InstructionEntity *ins)    // 0x3E (Z 0 0 C) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = reg_srl_8(ins->low);
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool srl_a(InstructionEntity *ins)     // 0x3F (Z 0 0 C) 2M
{
    R->A = reg_srl_8(R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static void reg_bit_x(BitMask mask, uint8_t r)
{
    bool is_zero = ((r & mask) == 0);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(true, HALF_CARRY_FLAG);
}
static bool bit_0_b(InstructionEntity *ins)   // 0x40 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_0_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_0_c(InstructionEntity *ins)   // 0x41 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_0_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_0_d(InstructionEntity *ins)   // 0x42 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_0_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_0_e(InstructionEntity *ins)   // 0x43 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_0_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_0_h(InstructionEntity *ins)   // 0x44 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_0_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_0_l(InstructionEntity *ins)   // 0x45 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_0_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool bit_0_hl(InstructionEntity *ins)  // 0x46 (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_0_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_0_a(InstructionEntity *ins)   // 0x47 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_0_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool bit_1_b(InstructionEntity *ins)   // 0x48 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_1_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_1_c(InstructionEntity *ins)   // 0x49 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_1_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_1_d(InstructionEntity *ins)   // 0x4A (Z 0 1 -) 2M
{
    reg_bit_x(BIT_1_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_1_e(InstructionEntity *ins)   // 0x4B (Z 0 1 -) 2M
{
    reg_bit_x(BIT_1_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_1_h(InstructionEntity *ins)   // 0x4C (Z 0 1 -) 2M
{
    reg_bit_x(BIT_1_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_1_l(InstructionEntity *ins)   // 0x4D (Z 0 1 -) 2M
{
    reg_bit_x(BIT_1_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool bit_1_hl(InstructionEntity *ins)  // 0x4E (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_1_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_1_a(InstructionEntity *ins)   // 0x4F (Z 0 1 -) 2M
{
    reg_bit_x(BIT_1_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool bit_2_b(InstructionEntity *ins)   // 0x50 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_2_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_2_c(InstructionEntity *ins)   // 0x51 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_2_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_2_d(InstructionEntity *ins)   // 0x52 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_2_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_2_e(InstructionEntity *ins)   // 0x53 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_2_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_2_h(InstructionEntity *ins)   // 0x54 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_2_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_2_l(InstructionEntity *ins)   // 0x55 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_2_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool bit_2_hl(InstructionEntity *ins)  // 0x56 (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_2_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_2_a(InstructionEntity *ins)   // 0x57 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_2_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool bit_3_b(InstructionEntity *ins)   // 0x58 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_3_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_3_c(InstructionEntity *ins)   // 0x59 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_3_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_3_d(InstructionEntity *ins)   // 0x5A (Z 0 1 -) 2M
{
    reg_bit_x(BIT_3_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_3_e(InstructionEntity *ins)   // 0x5B (Z 0 1 -) 2M
{
    reg_bit_x(BIT_3_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_3_h(InstructionEntity *ins)   // 0x5C (Z 0 1 -) 2M
{
    reg_bit_x(BIT_3_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_3_l(InstructionEntity *ins)   // 0x5D (Z 0 1 -) 2M
{
    reg_bit_x(BIT_3_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool bit_3_hl(InstructionEntity *ins)  // 0x5E (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_3_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_3_a(InstructionEntity *ins)   // 0x5F (Z 0 1 -) 2M
{
    reg_bit_x(BIT_3_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool bit_4_b(InstructionEntity *ins)   // 0x60 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_4_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_4_c(InstructionEntity *ins)   // 0x61 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_4_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_4_d(InstructionEntity *ins)   // 0x62 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_4_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_4_e(InstructionEntity *ins)   // 0x63 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_4_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_4_h(InstructionEntity *ins)   // 0x64 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_4_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_4_l(InstructionEntity *ins)   // 0x65 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_4_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool bit_4_hl(InstructionEntity *ins)  // 0x66 (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_4_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_4_a(InstructionEntity *ins)   // 0x67 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_4_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool bit_5_b(InstructionEntity *ins)   // 0x68 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_5_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_5_c(InstructionEntity *ins)   // 0x69 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_5_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_5_d(InstructionEntity *ins)   // 0x6A (Z 0 1 -) 2M
{
    reg_bit_x(BIT_5_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_5_e(InstructionEntity *ins)   // 0x6B (Z 0 1 -) 2M
{
    reg_bit_x(BIT_5_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_5_h(InstructionEntity *ins)   // 0x6C (Z 0 1 -) 2M
{
    reg_bit_x(BIT_5_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_5_l(InstructionEntity *ins)   // 0x6D (Z 0 1 -) 2M
{
    reg_bit_x(BIT_5_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool bit_5_hl(InstructionEntity *ins)  // 0x6E (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_5_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_5_a(InstructionEntity *ins)   // 0x6F (Z 0 1 -) 2M
{
    reg_bit_x(BIT_5_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool bit_6_b(InstructionEntity *ins)   // 0x70 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_6_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_6_c(InstructionEntity *ins)   // 0x71 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_6_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_6_d(InstructionEntity *ins)   // 0x72 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_6_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_6_e(InstructionEntity *ins)   // 0x73 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_6_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_6_h(InstructionEntity *ins)   // 0x74 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_6_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_6_l(InstructionEntity *ins)   // 0x75 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_6_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool bit_6_hl(InstructionEntity *ins)  // 0x76 (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_6_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_6_a(InstructionEntity *ins)   // 0x77 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_6_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool bit_7_b(InstructionEntity *ins)   // 0x78 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_7_MASK, R->B);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool bit_7_c(InstructionEntity *ins)   // 0x79 (Z 0 1 -) 2M
{
    reg_bit_x(BIT_7_MASK, R->C);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool bit_7_d(InstructionEntity *ins)   // 0x7A (Z 0 1 -) 2M
{
    reg_bit_x(BIT_7_MASK, R->D);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool bit_7_e(InstructionEntity *ins)   // 0x7B (Z 0 1 -) 2M
{
    reg_bit_x(BIT_7_MASK, R->E);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool bit_7_h(InstructionEntity *ins)   // 0x7C (Z 0 1 -) 2M
{
    reg_bit_x(BIT_7_MASK, R->H);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool bit_7_l(InstructionEntity *ins)   // 0x7D (Z 0 1 -) 2M
{
    reg_bit_x(BIT_7_MASK, R->L);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;;
}
static bool bit_7_hl(InstructionEntity *ins)  // 0x7E (Z 0 1 -) 3M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {
        ins->address = getDR(HL_REG);
        ins->    low = read_memory(ins->address);
        reg_bit_x(BIT_7_MASK, ins->low);
        cpu_log(DEBUG, "[$%04X] - %d", ins->address, is_flag_set(ZERO_FLAG));
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool bit_7_a(InstructionEntity *ins)   // 0x7F (Z 0 1 -) 2M
{
    reg_bit_x(BIT_7_MASK, R->A);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static bool res_0_b(InstructionEntity *ins)   // 0x80 (- - - -) 2M
{
    R->B = (R->B & ~BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_0_c(InstructionEntity *ins)   // 0x81 (- - - -) 2M
{
    R->C = (R->C & ~BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_0_d(InstructionEntity *ins)   // 0x82 (- - - -) 2M
{
    R->D = (R->D & ~BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_0_e(InstructionEntity *ins)   // 0x83 (- - - -) 2M
{
    R->E = (R->E & ~BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_0_h(InstructionEntity *ins)   // 0x84 (- - - -) 2M
{
    R->H = (R->H & ~BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_0_l(InstructionEntity *ins)   // 0x85 (- - - -) 2M
{
    R->L = (R->L & ~BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_0_hl(InstructionEntity *ins)  // 0x86 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_0_MASK)); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_0_a(InstructionEntity *ins)   // 0x87 (- - - -) 2M
{
    R->A = (R->A & ~BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool res_1_b(InstructionEntity *ins)   // 0x88 (- - - -) 2M
{
    R->B = (R->B & ~BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_1_c(InstructionEntity *ins)   // 0x89 (- - - -) 2M
{
    R->C = (R->C & ~BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_1_d(InstructionEntity *ins)   // 0x8A (- - - -) 2M
{
    R->D = (R->D & ~BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_1_e(InstructionEntity *ins)   // 0x8B (- - - -) 2M
{
    R->E = (R->E & ~BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_1_h(InstructionEntity *ins)   // 0x8C (- - - -) 2M
{
    R->H = (R->H & ~BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_1_l(InstructionEntity *ins)   // 0x8D (- - - -) 2M
{
    R->L = (R->L & ~BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_1_hl(InstructionEntity *ins)  // 0x8E (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_1_MASK));
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_1_a(InstructionEntity *ins)   // 0x8F (- - - -) 2M
{
    R->A = (R->A & ~BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool res_2_b(InstructionEntity *ins)   // 0x90 (- - - -) 2M
{
    R->B = (R->B & ~BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_2_c(InstructionEntity *ins)   // 0x91 (- - - -) 2M
{
    R->C = (R->C & ~BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_2_d(InstructionEntity *ins)   // 0x92 (- - - -) 2M
{
    R->D = (R->D & ~BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_2_e(InstructionEntity *ins)   // 0x93 (- - - -) 2M
{
    R->E = (R->E & ~BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_2_h(InstructionEntity *ins)   // 0x94 (- - - -) 2M
{
    R->H = (R->H & ~BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_2_l(InstructionEntity *ins)   // 0x95 (- - - -) 2M
{
    R->L = (R->L & ~BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_2_hl(InstructionEntity *ins)  // 0x96 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_2_MASK)); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_2_a(InstructionEntity *ins)   // 0x97 (- - - -) 2M
{
    R->A = (R->A & ~BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool res_3_b(InstructionEntity *ins)   // 0x98 (- - - -) 2M
{
    R->B = (R->B & ~BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_3_c(InstructionEntity *ins)   // 0x99 (- - - -) 2M
{
    R->C = (R->C & ~BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_3_d(InstructionEntity *ins)   // 0x9A (- - - -) 2M
{
    R->D = (R->D & ~BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_3_e(InstructionEntity *ins)   // 0x9B (- - - -) 2M
{
    R->E = (R->E & ~BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_3_h(InstructionEntity *ins)   // 0x9C (- - - -) 2M
{
    R->H = (R->H & ~BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_3_l(InstructionEntity *ins)   // 0x9D (- - - -) 2M
{
    R->L = (R->L & ~BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_3_hl(InstructionEntity *ins)  // 0x9E (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_3_MASK)); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_3_a(InstructionEntity *ins)   // 0x9F (- - - -) 2M
{
    R->A = (R->A & ~BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool res_4_b(InstructionEntity *ins)   // 0xA0 (- - - -) 2M
{
    R->B = (R->B & ~BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_4_c(InstructionEntity *ins)   // 0xA1 (- - - -) 2M
{
    R->C = (R->C & ~BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_4_d(InstructionEntity *ins)   // 0xA2 (- - - -) 2M
{
    R->D = (R->D & ~BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_4_e(InstructionEntity *ins)   // 0xA3 (- - - -) 2M
{
    R->E = (R->E & ~BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_4_h(InstructionEntity *ins)   // 0xA4 (- - - -) 2M
{
    R->H = (R->H & ~BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_4_l(InstructionEntity *ins)   // 0xA5 (- - - -) 2M
{
    R->L = (R->L & ~BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_4_hl(InstructionEntity *ins)  // 0xA6 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_4_MASK)); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_4_a(InstructionEntity *ins)   // 0xA7 (- - - -) 2M
{
    R->A = (R->A & ~BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool res_5_b(InstructionEntity *ins)   // 0xA8 (- - - -) 2M
{
    R->B = (R->B & ~BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_5_c(InstructionEntity *ins)   // 0xA9 (- - - -) 2M
{
    R->C = (R->C & ~BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_5_d(InstructionEntity *ins)   // 0xAA (- - - -) 2M
{
    R->D = (R->D & ~BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_5_e(InstructionEntity *ins)   // 0xAB (- - - -) 2M
{
    R->E = (R->E & ~BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_5_h(InstructionEntity *ins)   // 0xAC (- - - -) 2M
{
    R->H = (R->H & ~BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_5_l(InstructionEntity *ins)   // 0xAD (- - - -) 2M
{
    R->L = (R->L & ~BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_5_hl(InstructionEntity *ins)  // 0xAE (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_5_MASK)); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_5_a(InstructionEntity *ins)   // 0xAF (- - - -) 2M
{
    R->A = (R->A & ~BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool res_6_b(InstructionEntity *ins)   // 0xB0 (- - - -) 2M
{
    R->B = (R->B & ~BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_6_c(InstructionEntity *ins)   // 0xB1 (- - - -) 2M
{
    R->C = (R->C & ~BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_6_d(InstructionEntity *ins)   // 0xB2 (- - - -) 2M
{
    R->D = (R->D & ~BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_6_e(InstructionEntity *ins)   // 0xB3 (- - - -) 2M
{
    R->E = (R->E & ~BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_6_h(InstructionEntity *ins)   // 0xB4 (- - - -) 2M
{
    R->H = (R->H & ~BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_6_l(InstructionEntity *ins)   // 0xB5 (- - - -) 2M
{
    R->L = (R->L & ~BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_6_hl(InstructionEntity *ins)  // 0xB6 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_6_MASK)); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_6_a(InstructionEntity *ins)   // 0xB7 (- - - -) 2M
{
    R->A = (R->A & ~BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool res_7_b(InstructionEntity *ins)   // 0xB8 (- - - -) 2M
{
    R->B = (R->B & ~BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool res_7_c(InstructionEntity *ins)   // 0xB9 (- - - -) 2M
{
    R->C = (R->C & ~BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool res_7_d(InstructionEntity *ins)   // 0xBA (- - - -) 2M
{
    R->D = (R->D & ~BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool res_7_e(InstructionEntity *ins)   // 0xBB (- - - -) 2M
{
    R->E = (R->E & ~BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool res_7_h(InstructionEntity *ins)   // 0xBC (- - - -) 2M
{
    R->H = (R->H & ~BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool res_7_l(InstructionEntity *ins)   // 0xBD (- - - -) 2M
{
    R->L = (R->L & ~BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool res_7_hl(InstructionEntity *ins)  // 0xBE (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low & (~BIT_7_MASK)); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool res_7_a(InstructionEntity *ins)   // 0xBF (- - - -) 2M
{
    R->A = (R->A & ~BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
// Checked
static bool set_0_b(InstructionEntity *ins)   // 0xC0 (- - - -) 2M
{
    R->B = (R->B | BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_0_c(InstructionEntity *ins)   // 0xC1 (- - - -) 2M
{
    R->C = (R->C | BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_0_d(InstructionEntity *ins)   // 0xC2 (- - - -) 2M
{
    R->D = (R->D | BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_0_e(InstructionEntity *ins)   // 0xC3 (- - - -) 2M
{
    R->E = (R->E | BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_0_h(InstructionEntity *ins)   // 0xC4 (- - - -) 2M
{
    R->H = (R->H | BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_0_l(InstructionEntity *ins)   // 0xC5 (- - - -) 2M
{
    R->L = (R->L | BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_0_hl(InstructionEntity *ins)  // 0xC6 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_0_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_0_a(InstructionEntity *ins)   // 0xC7 (- - - -) 2M
{
    R->A = (R->A | BIT_0_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool set_1_b(InstructionEntity *ins)   // 0xC8 (- - - -) 2M
{
    R->B = (R->B | BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_1_c(InstructionEntity *ins)   // 0xC9 (- - - -) 2M
{
    R->C = (R->C | BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_1_d(InstructionEntity *ins)   // 0xCA (- - - -) 2M
{
    R->D = (R->D | BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_1_e(InstructionEntity *ins)   // 0xCB (- - - -) 2M
{
    R->E = (R->E | BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_1_h(InstructionEntity *ins)   // 0xCC (- - - -) 2M
{
    R->H = (R->H | BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_1_l(InstructionEntity *ins)   // 0xCD (- - - -) 2M
{
    R->L = (R->L | BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_1_hl(InstructionEntity *ins)  // 0xCE (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_1_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_1_a(InstructionEntity *ins)   // 0xCF (- - - -) 2M
{
    R->A = (R->A | BIT_1_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool set_2_b(InstructionEntity *ins)   // 0xD0 (- - - -) 2M
{
    R->B = (R->B | BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_2_c(InstructionEntity *ins)   // 0xD1 (- - - -) 2M
{
    R->C = (R->C | BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_2_d(InstructionEntity *ins)   // 0xD2 (- - - -) 2M
{
    R->D = (R->D | BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_2_e(InstructionEntity *ins)   // 0xD3 (- - - -) 2M
{
    R->E = (R->E | BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_2_h(InstructionEntity *ins)   // 0xD4 (- - - -) 2M
{
    R->H = (R->H | BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_2_l(InstructionEntity *ins)   // 0xD5 (- - - -) 2M
{
    R->L = (R->L | BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_2_hl(InstructionEntity *ins)  // 0xD6 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_2_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_2_a(InstructionEntity *ins)   // 0xD7 (- - - -) 2M
{
    R->A = (R->A | BIT_2_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool set_3_b(InstructionEntity *ins)   // 0xD8 (- - - -) 2M
{
    R->B = (R->B | BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_3_c(InstructionEntity *ins)   // 0xD9 (- - - -) 2M
{
    R->C = (R->C | BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_3_d(InstructionEntity *ins)   // 0xDA (- - - -) 2M
{
    R->D = (R->D | BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_3_e(InstructionEntity *ins)   // 0xDB (- - - -) 2M
{
    R->E = (R->E | BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_3_h(InstructionEntity *ins)   // 0xDC (- - - -) 2M
{
    R->H = (R->H | BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_3_l(InstructionEntity *ins)   // 0xDD (- - - -) 2M
{
    R->L = (R->L | BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_3_hl(InstructionEntity *ins)  // 0xDE (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_3_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_3_a(InstructionEntity *ins)   // 0xDF (- - - -) 2M
{
    R->A = (R->A | BIT_3_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool set_4_b(InstructionEntity *ins)   // 0xE0 (- - - -) 2M
{
    R->B = (R->B | BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_4_c(InstructionEntity *ins)   // 0xE1 (- - - -) 2M
{
    R->C = (R->C | BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_4_d(InstructionEntity *ins)   // 0xE2 (- - - -) 2M
{
    R->D = (R->D | BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_4_e(InstructionEntity *ins)   // 0xE3 (- - - -) 2M
{
    R->E = (R->E | BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_4_h(InstructionEntity *ins)   // 0xE4 (- - - -) 2M
{
    R->H = (R->H | BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_4_l(InstructionEntity *ins)   // 0xE5 (- - - -) 2M
{
    R->L = (R->L | BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_4_hl(InstructionEntity *ins)  // 0xE6 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_4_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_4_a(InstructionEntity *ins)   // 0xE7 (- - - -) 2M
{
    R->A = (R->A | BIT_4_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool set_5_b(InstructionEntity *ins)   // 0xE8 (- - - -) 2M
{
    R->B = (R->B | BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_5_c(InstructionEntity *ins)   // 0xE9 (- - - -) 2M
{
    R->C = (R->C | BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_5_d(InstructionEntity *ins)   // 0xEA (- - - -) 2M
{
    R->D = (R->D | BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_5_e(InstructionEntity *ins)   // 0xEB (- - - -) 2M
{
    R->E = (R->E | BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_5_h(InstructionEntity *ins)   // 0xEC (- - - -) 2M
{
    R->H = (R->H | BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_5_l(InstructionEntity *ins)   // 0xED (- - - -) 2M
{
    R->L = (R->L | BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_5_hl(InstructionEntity *ins)  // 0xEE (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_5_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_5_a(InstructionEntity *ins)   // 0xEF (- - - -) 2M
{
    R->A = (R->A | BIT_5_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool set_6_b(InstructionEntity *ins)   // 0xF0 (- - - -) 2M
{
    R->B = (R->B | BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_6_c(InstructionEntity *ins)   // 0xF1 (- - - -) 2M
{
    R->C = (R->C | BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_6_d(InstructionEntity *ins)   // 0xF2 (- - - -) 2M
{
    R->D = (R->D | BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_6_e(InstructionEntity *ins)   // 0xF3 (- - - -) 2M
{
    R->E = (R->E | BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_6_h(InstructionEntity *ins)   // 0xF4 (- - - -) 2M
{
    R->H = (R->H | BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_6_l(InstructionEntity *ins)   // 0xF5 (- - - -) 2M
{
    R->L = (R->L | BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_6_hl(InstructionEntity *ins)  // 0xF6 (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_6_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_6_a(InstructionEntity *ins)   // 0xF7 (- - - -) 2M
{
    R->A = (R->A | BIT_6_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}
static bool set_7_b(InstructionEntity *ins)   // 0xF8 (- - - -) 2M
{
    R->B = (R->B | BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->B);
    return true;
}
static bool set_7_c(InstructionEntity *ins)   // 0xF9 (- - - -) 2M
{
    R->C = (R->C | BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->C);
    return true;
}
static bool set_7_d(InstructionEntity *ins)   // 0xFA (- - - -) 2M
{
    R->D = (R->D | BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->D);
    return true;
}
static bool set_7_e(InstructionEntity *ins)   // 0xFB (- - - -) 2M
{
    R->E = (R->E | BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->E);
    return true;
}
static bool set_7_h(InstructionEntity *ins)   // 0xFC (- - - -) 2M
{
    R->H = (R->H | BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->H);
    return true;
}
static bool set_7_l(InstructionEntity *ins)   // 0xFD (- - - -) 2M
{
    R->L = (R->L | BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->L);
    return true;
}
static bool set_7_hl(InstructionEntity *ins)  // 0xFE (- - - -) 4M
{
    if (ins->duration == 1) // First Cycle
    {
        cpu_log(DEBUG, "...");
        return false;
    }

    if (ins->duration == 2) // Second Cycle
    {  
        ins->address = getDR(HL_REG);
        ins->low = read_memory(ins->address);
        cpu_log(DEBUG, "Read $%02X from [$%04X]", ins->low, ins->address);
        return false;
    }

    if (ins->duration == 3) // Third Cycle
    {
        uint8_t result = (ins->low | BIT_7_MASK); 
        write_memory(ins->address, result);
        cpu_log(DEBUG, "Wrote $%02X to [$%04X]", result, ins->address);
        return true; // Instruction Complete
    }
    
    cpu_log(ERROR, "Invalid operation, moving on.");
    return true;
}
static bool set_7_a(InstructionEntity *ins)   // 0xFF (- - - -) 2M
{
    R->A = (R->A | BIT_7_MASK);
    cpu_log(DEBUG, "Result $%02X", R->A);
    return true;
}

const OpcodeHandler prefix_opcode_table[256] = 
{
    /*              ROW 1               */
    [0x00] = rlc_b,   
    [0x01] = rlc_c,
    [0x02] = rlc_d,
    [0x03] = rlc_e,
    [0x04] = rlc_h,
    [0x05] = rlc_l,
    [0x06] = rlc_hl,
    [0x07] = rlc_a,
    [0x08] = rrc_b,
    [0x09] = rrc_c,
    [0x0A] = rrc_d,
    [0x0B] = rrc_e,
    [0x0C] = rrc_h,
    [0x0D] = rrc_l,
    [0x0E] = rrc_hl,
    [0x0F] = rrc_a,
    /*              ROW 2                */
    [0x10] = rl_b,   
    [0x11] = rl_c,
    [0x12] = rl_d,
    [0x13] = rl_e,
    [0x14] = rl_h,
    [0x15] = rl_l,
    [0x16] = rl_hl,
    [0x17] = rl_a,
    [0x18] = rr_b,
    [0x19] = rr_c,
    [0x1A] = rr_d,
    [0x1B] = rr_e,
    [0x1C] = rr_h,
    [0x1D] = rr_l,
    [0x1E] = rr_hl,
    [0x1F] = rr_a,
    /*              ROW 3                */
    [0x20] = sla_b,   
    [0x21] = sla_c,
    [0x22] = sla_d,
    [0x23] = sla_e,
    [0x24] = sla_h,
    [0x25] = sla_l,
    [0x26] = sla_hl,
    [0x27] = sla_a,
    [0x28] = sra_b,
    [0x29] = sra_c,
    [0x2A] = sra_d,
    [0x2B] = sra_e,
    [0x2C] = sra_h,
    [0x2D] = sra_l,
    [0x2E] = sra_hl,
    [0x2F] = sra_a,
    /*              ROW 4                */
    [0x30] = swap_b,   
    [0x31] = swap_c,
    [0x32] = swap_d,
    [0x33] = swap_e,
    [0x34] = swap_h,
    [0x35] = swap_l,
    [0x36] = swap_hl,
    [0x37] = swap_a,
    [0x38] = srl_b,
    [0x39] = srl_c,
    [0x3A] = srl_d,
    [0x3B] = srl_e,
    [0x3C] = srl_h,
    [0x3D] = srl_l,
    [0x3E] = srl_hl,
    [0x3F] = srl_a,
    /*              ROW 5                */
    [0x40] = bit_0_b,   
    [0x41] = bit_0_c,
    [0x42] = bit_0_d,
    [0x43] = bit_0_e,
    [0x44] = bit_0_h,
    [0x45] = bit_0_l,
    [0x46] = bit_0_hl,
    [0x47] = bit_0_a,
    [0x48] = bit_1_b,
    [0x49] = bit_1_c,
    [0x4A] = bit_1_d,
    [0x4B] = bit_1_e,
    [0x4C] = bit_1_h,
    [0x4D] = bit_1_l,
    [0x4E] = bit_1_hl,
    [0x4F] = bit_1_a,
    /*              ROW 6                */
    [0x50] = bit_2_b,   
    [0x51] = bit_2_c,
    [0x52] = bit_2_d,
    [0x53] = bit_2_e,
    [0x54] = bit_2_h,
    [0x55] = bit_2_l,
    [0x56] = bit_2_hl,
    [0x57] = bit_2_a,
    [0x58] = bit_3_b,
    [0x59] = bit_3_c,
    [0x5A] = bit_3_d,
    [0x5B] = bit_3_e,
    [0x5C] = bit_3_h,
    [0x5D] = bit_3_l,
    [0x5E] = bit_3_hl,
    [0x5F] = bit_3_a,
    /*              ROW 7                */
    [0x60] = bit_4_b,   
    [0x61] = bit_4_c,
    [0x62] = bit_4_d,
    [0x63] = bit_4_e,
    [0x64] = bit_4_h,
    [0x65] = bit_4_l,
    [0x66] = bit_4_hl,
    [0x67] = bit_4_a,
    [0x68] = bit_5_b,
    [0x69] = bit_5_c,
    [0x6A] = bit_5_d,
    [0x6B] = bit_5_e,
    [0x6C] = bit_5_h,
    [0x6D] = bit_5_l,
    [0x6E] = bit_5_hl,
    [0x6F] = bit_5_a,
    /*              ROW 8                */
    [0x70] = bit_6_b,   
    [0x71] = bit_6_c,
    [0x72] = bit_6_d,
    [0x73] = bit_6_e,
    [0x74] = bit_6_h,
    [0x75] = bit_6_l,
    [0x76] = bit_6_hl,
    [0x77] = bit_6_a,
    [0x78] = bit_7_b,
    [0x79] = bit_7_c,
    [0x7A] = bit_7_d,
    [0x7B] = bit_7_e,
    [0x7C] = bit_7_h,
    [0x7D] = bit_7_l,
    [0x7E] = bit_7_hl,
    [0x7F] = bit_7_a,
    /*              ROW 9                */
    [0x80] = res_0_b,   
    [0x81] = res_0_c,
    [0x82] = res_0_d,
    [0x83] = res_0_e,
    [0x84] = res_0_h,
    [0x85] = res_0_l,
    [0x86] = res_0_hl,
    [0x87] = res_0_a,
    [0x88] = res_1_b,
    [0x89] = res_1_c,
    [0x8A] = res_1_d,
    [0x8B] = res_1_e,
    [0x8C] = res_1_h,
    [0x8D] = res_1_l,
    [0x8E] = res_1_hl,
    [0x8F] = res_1_a,
    /*              ROW 10                */
    [0x90] = res_2_b,   
    [0x91] = res_2_c,
    [0x92] = res_2_d,
    [0x93] = res_2_e,
    [0x94] = res_2_h,
    [0x95] = res_2_l,
    [0x96] = res_2_hl,
    [0x97] = res_2_a,
    [0x98] = res_3_b,
    [0x99] = res_3_c,
    [0x9A] = res_3_d,
    [0x9B] = res_3_e,
    [0x9C] = res_3_h,
    [0x9D] = res_3_l,
    [0x9E] = res_3_hl,
    [0x9F] = res_3_a,
    /*              ROW 11                */
    [0xA0] = res_4_b,   
    [0xA1] = res_4_c,
    [0xA2] = res_4_d,
    [0xA3] = res_4_e,
    [0xA4] = res_4_h,
    [0xA5] = res_4_l,
    [0xA6] = res_4_hl,
    [0xA7] = res_4_a,
    [0xA8] = res_5_b,
    [0xA9] = res_5_c,
    [0xAA] = res_5_d,
    [0xAB] = res_5_e,
    [0xAC] = res_5_h,
    [0xAD] = res_5_l,
    [0xAE] = res_5_hl,
    [0xAF] = res_5_a,
    /*              ROW 12                */
    [0xB0] = res_6_b,   
    [0xB1] = res_6_c,
    [0xB2] = res_6_d,
    [0xB3] = res_6_e,
    [0xB4] = res_6_h,
    [0xB5] = res_6_l,
    [0xB6] = res_6_hl,
    [0xB7] = res_6_a,
    [0xB8] = res_7_b,
    [0xB9] = res_7_c,
    [0xBA] = res_7_d,
    [0xBB] = res_7_e,
    [0xBC] = res_7_h,
    [0xBD] = res_7_l,
    [0xBE] = res_7_hl,
    [0xBF] = res_7_a,
    /*              ROW 13                */
    [0xC0] = set_0_b,   
    [0xC1] = set_0_c,
    [0xC2] = set_0_d,
    [0xC3] = set_0_e,
    [0xC4] = set_0_h,
    [0xC5] = set_0_l,
    [0xC6] = set_0_hl,
    [0xC7] = set_0_a,
    [0xC8] = set_1_b,
    [0xC9] = set_1_c,
    [0xCA] = set_1_d,
    [0xCB] = set_1_e,
    [0xCC] = set_1_h,
    [0xCD] = set_1_l,
    [0xCE] = set_1_hl,
    [0xCF] = set_1_a,
    /*              ROW 14                */
    [0xD0] = set_2_b,   
    [0xD1] = set_2_c,
    [0xD2] = set_2_d,
    [0xD3] = set_2_e,
    [0xD4] = set_2_h,
    [0xD5] = set_2_l,
    [0xD6] = set_2_hl,
    [0xD7] = set_2_a,
    [0xD8] = set_3_b,
    [0xD9] = set_3_c,
    [0xDA] = set_3_d,
    [0xDB] = set_3_e,
    [0xDC] = set_3_h,
    [0xDD] = set_3_l,
    [0xDE] = set_3_hl,
    [0xDF] = set_3_a,
    /*              ROW 15                */
    [0xE0] = set_4_b,   
    [0xE1] = set_4_c,
    [0xE2] = set_4_d,
    [0xE3] = set_4_e,
    [0xE4] = set_4_h,
    [0xE5] = set_4_l,
    [0xE6] = set_4_hl,
    [0xE7] = set_4_a,
    [0xE8] = set_5_b,
    [0xE9] = set_5_c,
    [0xEA] = set_5_d,
    [0xEB] = set_5_e,
    [0xEC] = set_5_h,
    [0xED] = set_5_l,
    [0xEE] = set_5_hl,
    [0xEF] = set_5_a,
    /*              ROW 16                */
    [0xF0] = set_6_b,   
    [0xF1] = set_6_c,
    [0xF2] = set_6_d,
    [0xF3] = set_6_e,
    [0xF4] = set_6_h,
    [0xF5] = set_6_l,
    [0xF6] = set_6_hl,
    [0xF7] = set_6_a,
    [0xF8] = set_7_b,
    [0xF9] = set_7_c,
    [0xFA] = set_7_d,
    [0xFB] = set_7_e,
    [0xFC] = set_7_h,
    [0xFD] = set_7_l,
    [0xFE] = set_7_hl,
    [0xFF] = set_7_a,
};

static void reset_ins(InstructionEntity *ins)
{
    ins-> address = R->PC;
    ins->duration =     0;
    ins->  length =     1;
    ins->     low =     0;
    ins->    high =     0;
    ins->  opcode =     0;
    ins->   label = "N/A";
    ins->executed = false;
    ins-> handler =   nop;
}

/* INTERRUPT HANDLING */

static void check_ime(InterruptEnableEvent *iee)
{
    if (iee->active)
    { // Interrupt Enable Check
        if (--iee->delay <= 0)
        {
            cpu->   ime = true;
            iee->active = false;
            cpu_log(DEBUG, "IME Enabled");
        }
    }
}

void request_interrupt(InterruptCode interrupt)
{
    write_ifr(*R->IFR | interrupt);
    cpu_log(DEBUG, "Requesting Interrupt %02X", interrupt);
}

static bool service_interrupts(InstructionEntity *ins)
{
    uint8_t pending = get_pending_interrupts();
    if ((!cpu->ime) || (pending == 0)) return false;

    cpu->ime = false;
    reset_ins(ins); // Normalize
    ins->handler = int_exec;

    if (pending & VBLANK_INTERRUPT_CODE)
    {
        ins->address = VBLANK_VECTOR;
        ins->  label = "VBLANK INTTERRUPT";
        ins->    low = VBLANK_INTERRUPT_CODE; // Confirm servicing
        return true;
    }
    if (pending & LCD_STAT_INTERRUPT_CODE)
    {
        ins->address = LCD_VECTOR;
        ins->  label = "LCD INTTERRUPT";
        ins->    low =  LCD_STAT_INTERRUPT_CODE; // Confirm servicing
        return true;
    }
    if (pending & TIMER_INTERRUPT_CODE)
    {
        ins->address = TIMER_VECTOR;
        ins->  label = "TIMER INTTERRUPT";
        ins->    low = TIMER_INTERRUPT_CODE; // Confirm servicing
        return true;
    }
    if (pending & SERIAL_INTERRUPT_CODE)
    {
        ins->address = SERIAL_VECTOR;
        ins->  label = "SERIAL INTTERRUPT";
        ins->    low = SERIAL_INTERRUPT_CODE; // Confirm servicing
        return true;
    }
    if (pending & JOYPAD_INTERRUPT_CODE)
    {
        ins->address = JOYPAD_VECTOR;
        ins->  label = "JOYPAD INTTERRUPT";
        ins->    low = JOYPAD_INTERRUPT_CODE; // Confirm servicing
        return true;
    }

    cpu_log(ERROR, "Failed To Service Interrupt");
    return false;
}

/* INSTRUCTION EXECUTION */

static void check_pending_interrupts()
{
    if (cpu->halted) // Need to unhalt if interrupt pending.
    {
        uint8_t pending = get_pending_interrupts();
        cpu->halted = (pending == 0); // No reason to unhalt.
    }
}

static void next_ins(InstructionEntity *ins)
{
    reset_ins(ins);
    ins-> opcode = fetch();
    if (cb_prefixed)
    {
        ins->   label = cb_opcode_word[ins->opcode];
        ins-> handler = prefix_opcode_table[ins->opcode];
        cb_prefixed   = false;
        return;
    }
    ins->  label = opcode_word[ins->opcode];
    ins->handler = opcode_table[ins->opcode];
}

static void execute_ins(InstructionEntity *ins)
{
    ins->duration += 1; // Steps through micro-instructions
    bool complete = ins->handler(ins);
    if (complete)
    {
        check_ime(iee);
        if (!cb_prefixed)
        {
            bool serviced = service_interrupts(ins); 
            if (serviced) return;
        }
        next_ins(ins);
    }
}

void machine_cycle()
{
    check_pending_interrupts(); // Unhalts If Interrupt Pending
    
    if (!cpu->running) return;
    if   (cpu->halted) return;

    execute_ins(ins); // Continue Execution. 
}

/* OTHER PUBLIC METHODS */

uint8_t get_machine_cycle_scaler()
{
   uint8_t mcs = cpu->speed_enabled ? M2S_DOUBLE_SPEED : M2S_BASE_SPEED;
   return mcs;
}

void reset_cpu()
{
    R->PC = 0x0000;
}

void start_cpu()
{
    cpu->running = true;
}

void stop_cpu()
{
    cpu->running = false;
}

bool cpu_running()
{
    return cpu->running;
}

void init_cpu()
{
    // Init Pointers
    cpu = (CPU*)                                   malloc(sizeof(CPU));
    R   = (Register*)                         malloc(sizeof(Register));
    iee = (InterruptEnableEvent*) malloc(sizeof(InterruptEnableEvent));
    ins = (InstructionEntity*)       malloc(sizeof(InstructionEntity));
    reset_ins(ins); // Will Execute the first NOP
    iee->active = false;
    // Init CPU Registers
    R->A = R->F = DEFAULT_REG_VAL;
    R->B = R->C = DEFAULT_REG_VAL;
    R->D = R->E = DEFAULT_REG_VAL;
    R->H = R->L = DEFAULT_REG_VAL;
    // Init Hardware Registers
    R->IER = get_memory_pointer(IER);
    R->IFR = get_memory_pointer(IFR);
    // Init CPU State
    cpu->ime             = false;
    cpu->speed_enabled   = false;
    cpu->running         = true;
    cpu->halted          = false;
    cpu->halt_bug_active = false;
    // Init SP and PC.
    R->PC = 0x0000;
    R->SP = HIGH_RAM_ADDRESS_END;

}

void tidy_cpu()
{
    free(cpu); cpu = NULL;
    free(R);     R = NULL;
    free(iee); iee = NULL;
    free(ins); ins = NULL;
}



