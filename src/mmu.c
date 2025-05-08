#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "common.h"
#include "emulator.h"
#include "mmu.h"
#include "cpu.h"
#include "logger.h"
#include "cart.h"
#include "timer.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

#define MEMORY_SIZE       0x10000
#define CRAM_BANK_SIZE        128
#define ECHO_RAM_OFFSET    0x2000
#define DMA_DURATION          160
#define VRAM_BANK_SIZE     0x2000
#define VRAM_BANK_QUANTITY      2
#define WRAM_BANK_SIZE     0x1001
#define WRAM_BANK_QUANTITY      8 

typedef struct
{
    uint16_t src_address;
    uint16_t dst_address;
    uint8_t  cycles_left;
    bool          active;

} DMATransfer;

typedef struct
{
    uint16_t src_address;
    uint16_t dst_address;
    uint16_t      length;
    bool          active;

} HDMATransfer;

static HDMATransfer *hdma;
static DMATransfer   *dma;
static uint8_t    *memory;
static uint8_t      *cram;
static uint8_t     **vram;
static uint8_t     **wram;
static bool   bios_locked; 

void init_memory()
{
    // (65,536 Bytes) General Memory with some 'extra' room for lazy addressing.
    memory = (uint8_t*)  malloc(MEMORY_SIZE * sizeof(uint8_t));
    memset(memory, 0, MEMORY_SIZE); 
    // Default Values to 0. 

    // (128 Bytes) CRAM Memory 
    cram = (uint8_t*)  malloc(CRAM_BANK_SIZE * sizeof(uint8_t));
    // Defaults to random RGB Values.

    // DMA State Handlers
    dma  = (DMATransfer*)  malloc(sizeof(DMATransfer));
    hdma = (HDMATransfer*) malloc(sizeof(HDMATransfer));

    // (2 Banks ~8 KB) VRAM 
    vram    = (uint8_t**) malloc(VRAM_BANK_QUANTITY * sizeof(uint8_t*));
    vram[0] = (uint8_t*)  malloc(VRAM_BANK_SIZE     * sizeof(uint8_t));
    memset(vram[0], 0, VRAM_BANK_SIZE);
    vram[1] = (uint8_t*)  malloc(VRAM_BANK_SIZE     * sizeof(uint8_t));
    memset(vram[1], 0, VRAM_BANK_SIZE);
    // Defaut Values to 0, restrict access to bank 1 if not in CGB mode.

    // WRAM
    wram = (uint8_t**) malloc(WRAM_BANK_QUANTITY * sizeof(uint8_t*));
    for (uint8_t i = 0; i < WRAM_BANK_QUANTITY; i++)
    { // (Static Bank 0, 7 Banks ~4 KB), (SVBK)
        wram[i] = (uint8_t*) malloc(WRAM_BANK_SIZE * sizeof(uint8_t));
        memset(wram[i], 0, WRAM_BANK_SIZE); // Default values to 0.
    }

    bios_locked = false; // Latches when written.
}

void tidy_memory()
{
    free(memory);
    memory = NULL;

    free(cram);
    cram = NULL;

    free(dma);
    dma = NULL;
    free(hdma);
    hdma = NULL;

    free(vram[0]);
    vram[0] = NULL;
    free(vram[1]);
    vram[1] = NULL;
    free(vram);
    vram = NULL;

    for (int i = 0; i < WRAM_BANK_QUANTITY; i++)
    {
        free(wram[i]);
        wram[i] = NULL;
    }
    free(wram);
    wram = NULL;
}

/* DMA METHODOLOGY */

static void start_dma(uint8_t dma_val)
{
    memory[DMA] = dma_val;
    dma->src_address  = dma_val * 0x0100;
    dma->dst_address  = OAM_ADDRESS_START; 
    dma->cycles_left  = DMA_DURATION;
    dma->active       = true;
}

static void start_hdma(uint8_t hdma5)
{
    memory[HDMA5]     = hdma5;
    // SRC: HDMA1 (High), HDMA2 (Low)
    // Lower 4 are 0.
    hdma->src_address = (memory[HDMA1] << BYTE) | (memory[HDMA2] & 0xF0); 
    // DST: HDMA3 (High), HDMA4 (Low)
    // Only bits 12 - 4 are considered. 
    hdma->dst_address = 0x8000 | ((memory[HDMA3] & 0x1F) << BYTE) | (memory[HDMA4] & 0xF0);
    // Determine the length of transfer.
    // L = (R + 1) * $10
    hdma->length      = ((hdma5 & 0x7F) + 1) * 0x10;
    // Let's Yoink bit 7
    // 0 = General-Purpose DMA, 1 = HBlank DMA
    hdma->active      = hdma5 & BIT_7_MASK;
    if (hdma->active)
    { // General-Purpose DMA

        for (int i = 0; i < hdma->length; i++)
        { // Transfered all at once.
            memory[hdma->dst_address + i] = memory[hdma->src_address + i];
        }
        // Sets hdma5 to $FF.
        memory[HDMA5] = 0xFF; 
    }
    // HBlank DMA

    // Transfer Rate: $10 Bytes / HBlank
    // Transfer Timing: 32 dots. 
    // Halted during invdividual transfers.
    // Blocking, enforce with flag.
    // Transfer is halted alongside cpu. 
    // Reading returns remaining length.
    // Value of $FF indicataed transfer has completed. 
    // Can terminate early by writing 0 to Bit 7 of HDMA5. 
    // Stopping transfer does not set HDMA1-4 to $FF.
}

void check_dma()
{
    if(dma->active == 0) return;
    if(dma->cycles_left > 0)
    {
        uint8_t src_val = read_memory(dma->src_address++);
        write_memory(dma->dst_address++, src_val);
        dma->cycles_left -= 1;  
    }
    if(dma->cycles_left <= 0)
    {
        dma->active = false;
    }
}

bool dma_active()
{
    return (dma->active);
}

/* MEMORY ACCESS  */

uint8_t read_joypad()
{
    JoypadState *joypad = get_joypad();
    uint8_t select = memory[JOYP] & 0x30;
    uint8_t result = select | 0x0F;

    if ((select & BIT_5_MASK) == 0) // Action buttons selected
    {
        if (joypad->A)      result &= ~A_BUTTON_MASK;
        if (joypad->B)      result &= ~B_BUTTON_MASK;
        if (joypad->SELECT) result &= ~SELECT_BUTTON_MASK;
        if (joypad->START)  result &= ~START_BUTTON_MASK;
    }

    if ((select & BIT_4_MASK) == 0) // Direction buttons selected 
    {
        if (joypad->RIGHT) result &= ~RIGHT_BUTTON_MASK;
        if (joypad->LEFT)  result &= ~LEFT_BUTTON_MASK;
        if (joypad->UP)    result &= ~UP_BUTTON_MASK;
        if (joypad->DOWN)  result &= ~DOWN_BUTTON_MASK;
    }

    joypad_log(DEBUG, "|| (%02X):(%02X)", select, result);
    return result;
}


uint8_t io_memory_read(uint16_t address)
{
    if ((address < IO_REGISTERS_START) || (address > IO_REGISTERS_END))
    {
        LOG_MESSAGE(ERROR, "Invalid IO read attempt: %04X", address);
        return 0xFF;
    }
    switch (address)
    {
        case JOYP: return read_joypad();
        case BCPD: return cram[(memory[BCPS] & LOWER_6_MASK)];
        case OCPD: return cram[(memory[OCPS] & LOWER_6_MASK) + 0x40];
        default:   return memory[address];
    }
}

void io_memory_write(uint16_t address, uint8_t value)
{
    if ((address < IO_REGISTERS_START) || (address > IO_REGISTERS_END))
    {
        LOG_MESSAGE(ERROR, "Invalid IO write attempt: %04X", address);
        return;
    }
    switch(address)
    {
        case JOYP:
            memory[address] = (value & 0x30);
            break;
        case DIV:
            clear_sys();
            break;
        case TIMA:
            write_tima(value);
            break;
        case TAC:
            write_tac(value);
            break;
        case IFR:
            write_ifr(value);
            break;
        case DMA:
            start_dma(value);
            break;
        case KEY1:
            if (is_gbc()) memory[address] = value; 
            break;
        case VBK:
            if (is_gbc()) memory[address] = value; 
            break;
        case BIOS:
            if (!bios_locked)
            {
                memory[address] = value;
                bios_locked = true;
            }
            break;
        case HDMA1:
            if (is_gbc()) memory[address] = value; 
            break;
        case HDMA2:
            if (is_gbc()) memory[address] = value; 
            break;
        case HDMA3:
            if (is_gbc()) memory[address] = value; 
            break;
        case HDMA4:
            if (is_gbc()) memory[address] = value; 
            break;
        case HDMA5:
            if (is_gbc()) start_hdma(value);
            break;
        case RP:
            if (is_gbc()) memory[address] = value;
            break;
        case BCPS:
            if (is_gbc()) memory[address] = value;
            break;
        case BCPD:
            if (is_gbc())
            {
                uint8_t index = memory[BCPS] & LOWER_6_MASK;
                cram[index] = value;
                uint8_t inc_index = (index + 1) & LOWER_6_MASK;
                if(memory[BCPS] & BIT_7_MASK) 
                    memory[BCPS] = (memory[BCPS] & BIT_7_MASK) | (inc_index);
            }
            break;
        case OCPS:
            if (is_gbc()) memory[address] = value;
            break;
        case OCPD:
            if (is_gbc())
            {
                uint8_t index = memory[OCPS] & LOWER_6_MASK;
                cram[index + 0x40] = value;
                uint8_t inc_index = (index + 1) & LOWER_6_MASK;
                if (memory[OCPS] & BIT_7_MASK)
                    memory[OCPS] = (memory[OCPS] & BIT_7_MASK) | inc_index;
            }
            break;
        case OPRI:
            if (is_gbc()) memory[address] = value;
            break;
        case SVBK:
            if (is_gbc()) memory[address] = value;
            break;
        case PCM12:
            if (is_gbc()) memory[address] = value;
            break;
        case PCM34:
            if (is_gbc()) memory[address] = value;
            break;
        default:
            memory[address] = value;
            break;
    }
}

uint8_t read_vram(uint8_t bank, uint16_t address)
{
    if ((address < VRAM_ADDRESS_START) || (address > VRAM_ADDRESS_END))
    {
        LOG_MESSAGE(ERROR, "Invalid VRAM read attempt: %04X", address);
        return 0xFF;
    }
    address -= (uint16_t) VRAM_ADDRESS_START;
    return vram[bank][address];
}

uint8_t read_cram(bool is_obj, uint8_t palette_index, uint8_t color_id, uint8_t index)
{
    uint8_t base   = is_obj ? 0x40 : 0x00;
    uint8_t offset = (palette_index << 3) | (color_id << 1);
    return cram[base + offset + index];
}

uint8_t read_memory(uint16_t address)
{
    if (address <= BANK_N_ADDRESS_END)
    {
        return read_rom_memory(address); 
    }
    else if (address <= VRAM_ADDRESS_END)
    {
        uint8_t value = 
        memory[VBK] ? read_vram(1, address) : read_vram(0, address);
        return value;
    }
    else if (address <= EXT_RAM_ADDRESS_END)
    {
        return read_rom_memory(address);
    }
    else if (address <= WRAM_ZERO_ADDRESS_END)
    {
        address -= (uint16_t) WRAM_ZERO_ADDRESS_START;
        return wram[0][address];
    }
    else if (address <= WRAM_N_ADDRESS_END)
    {  
        uint8_t svbk = memory[SVBK] & LOWER_3_MASK;
        if (!svbk) svbk = 1;
        address -= (uint16_t) WRAM_N_ADDRESS_START;
        return wram[svbk][address];
    }
    else if (address <= ECHO_RAM_ADDRESS_END)
    {
        return read_memory(address - ECHO_RAM_OFFSET);
    }
    else if (address <= OAM_ADDRESS_END)
    {
        return memory[address];
    }
    else if (address <= NOT_USABLE_END)
    {
        return memory[address];
    }
    else if (address <= IO_REGISTERS_END)
    {
        return io_memory_read(address);
    }
    else if (address <= INTERRUPT_ENABLE_ADDRESS)
    { // Implicit High RAM
        return memory[address];
    }
    LOG_MESSAGE(ERROR, "Attempted Read from unmapped address: %04X", address);
    return 0xFF; // Return garbage if we ever get here.
}

void write_memory(uint16_t address, uint8_t value)
{ 
    if (address <= BANK_N_ADDRESS_END)
    {
        write_rom_memory(address, value);
        return;
    }
    else if (address <= VRAM_ADDRESS_END)
    {
        uint8_t bank = (is_gbc() && memory[VBK]) ? 1 : 0;
        address -= (uint16_t) VRAM_ADDRESS_START;
        vram[bank][address] = value;
        return;
    }
    else if (address <= EXT_RAM_ADDRESS_END)
    {
        write_rom_memory(address, value);
        return;
    }
    else if (address <= WRAM_ZERO_ADDRESS_END)
    {
        address -= (uint16_t) WRAM_ZERO_ADDRESS_START;
        wram[0][address] = value;
        return;
    }
    else if (address <= WRAM_N_ADDRESS_END)
    { // Banks 1-7
        uint8_t svbk = (memory[SVBK] & LOWER_3_MASK);
        if (!svbk) svbk = 1;
        address -= (uint16_t) WRAM_N_ADDRESS_START;
        wram[svbk][address] = value;
        return;
    }
    else if (address <= ECHO_RAM_ADDRESS_END)
    {
        write_memory(address - ECHO_RAM_OFFSET, value);
        return;
    }
    else if ((address >= IO_REGISTERS_START) && (address <= IO_REGISTERS_END))
    { // Bypass 'not usable' memory range
        io_memory_write(address, value);
        return;
    }
    else if (address <= HIGH_RAM_ADDRESS_END)
    { // implicit High RAM
        memory[address] = value;
        return;
    }
    else if (address == INTERRUPT_ENABLE_ADDRESS)
    {
        memory[address] = value;
        return;
    }
    LOG_MESSAGE(ERROR, "Attempted write to invalid address: %04X", address);
}

/* DEBUG */

uint8_t *get_memory()
{
    return memory;
}

uint8_t *get_memory_pointer(uint16_t address)
{
    return &(memory[address]);
}

void print_vram(uint16_t start, uint16_t end, bool bank)
{
    if (bank)
    {
        LOG_MESSAGE(INFO, "%s", "VRAM BANK 1 \n ----------------------------------");
        for (int i = start; i <= end; i++)
        {
            LOG_MESSAGE(INFO, "%04X: %02X", i, vram[1][i - VRAM_ADDRESS_START]);
        }
    }
    else
    {
        LOG_MESSAGE(INFO, "%s", "VRAM BANK 0 \n ----------------------------------");
        for (int i = start; i <= end; i++)
        {
            LOG_MESSAGE(INFO, "%04X: %02X", i, vram[0][i - VRAM_ADDRESS_START]);
        }
    }
}