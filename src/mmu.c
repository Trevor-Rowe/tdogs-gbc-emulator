#include <stdlib.h>
#include "common.h"
#include "mmu.h"
#include "ppu.h"
#include "logger.h"
#include "cart.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

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

static HDMATransfer       *hdma;
static DMATransfer         *dma;
static uint8_t          *memory;
static uint8_t            *cram;
static uint8_t           **vram;
static uint8_t           **wram;
static bool         bios_locked;   

/* DMA-Related Functionality */

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
    hdma->dst_address = B0_ADDRESS_START | ((memory[HDMA3] & 0x1F) << BYTE) | (memory[HDMA4] & 0xF0);
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

/* IO Register Functionality */

static uint8_t *io_memory_read(uint16_t address)
{
    if (address == BCPD) return &(cram[memory[BCPS] & LOWER_6_MASK]);
    if (address == OCPD) return &(cram[(memory[OCPS] & LOWER_6_MASK) + 0x40]);
    return &(memory[address]); 
}

static void io_memory_write(uint16_t address, uint8_t value)
{
    switch(address)
    {
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
                if(memory[BCPS] & BIT_7_MASK) memory[BCPS] = (memory[BCPS] & BIT_7_MASK) | ((index + 1) & LOWER_6_MASK);
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
                if(memory[OCPS] & BIT_7_MASK) memory[OCPS] = (memory[OCPS] & BIT_7_MASK) | ((index + 1) & LOWER_6_MASK);
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

/* Header Function Implementations */

void init_memory(uint16_t size)
{
    memory      = (uint8_t*)  malloc(size);
    vram        = (uint8_t**) malloc(VRAM_BANK_QUANTITY * sizeof(uint8_t*));
    wram        = (uint8_t**) malloc(WRAM_BANK_QUANTITY * sizeof(uint8_t*));
    cram        = (uint8_t*)  malloc(CRAM_BANK_SIZE);
    dma         = (DMATransfer*)  malloc(sizeof(DMATransfer));
    hdma        = (HDMATransfer*) malloc(sizeof(HDMATransfer));
    bios_locked = false;
    for (uint8_t i = 0; i < VRAM_BANK_QUANTITY; i++)
    {
        vram[i] = (uint8_t*) malloc(VRAM_BANK_SIZE * sizeof(uint8_t));
    }
    for (uint8_t i = 0; i < WRAM_BANK_QUANTITY; i++)
    {
        wram[i] = (uint8_t*) malloc(WRAM_BANK_SIZE * sizeof(uint8_t));
    }
}

void tidy_memory()
{
    free(memory);
    memory = NULL;
    for (uint8_t i = 0; i < VRAM_BANK_QUANTITY; i++)
    {
        free(vram[i]);
        vram[i] = NULL;
    }
    free(vram);
    vram = NULL;
    for (int i = 0; i < WRAM_BANK_QUANTITY; i++)
    {
        free(wram[i]);
        wram[i] = NULL;
    }
    free(wram);
    wram = NULL;
    free(hdma);
    hdma = NULL;
    free(dma);
    dma = NULL;
    free(cram);
    cram = NULL;
}

uint8_t *read_vram(uint8_t bank, uint16_t address)
{
    address -= (uint16_t) VRAM_ADDRESS_START;
    return &(vram[bank][address]);
}

uint8_t *read_cram(bool is_obj, uint8_t palette_index, uint8_t color_id)
{
    uint8_t base   = is_obj ? 0x40 : 0x00;
    uint8_t offset = (palette_index << 3) | (color_id << 1);
    return &(cram[base + offset]);
}

void check_dma()
{
    if(!dma->active) return;
    if(dma->cycles_left > 0)
    {
        uint8_t src_val = *read_memory(dma->src_address++);
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

uint8_t *get_memory()
{
    return memory;
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

uint8_t *read_memory(uint16_t address)
{
    if (address <= BANK_N_ADDRESS_END)
    {
        memory[address] = read_rom_memory(address);
        return &(memory[address]); 
    }
    else if (address <= VRAM_ADDRESS_END)
    {
        uint8_t *value = 
        (bool) memory[VBK] ? 
        &(vram[1][address - ((uint16_t) VRAM_ADDRESS_START)]):
        &(vram[0][address - ((uint16_t) VRAM_ADDRESS_START)]);
        return value;
    }
    else if (address <= EXT_RAM_ADDRESS_END)
    {
        uint8_t value = read_rom_memory(address);
        memory[address] = value;
        return &(memory[address]); 
    }
    else if (address <= WRAM_ZERO_ADDRESS_END)
    {
        address -= (uint16_t) WRAM_ZERO_ADDRESS_START;
        return &(wram[0][address]);
    }
    else if (address <= WRAM_N_ADDRESS_END)
    {  
        uint8_t svbk = memory[SVBK] & LOWER_3_MASK;
        if (!svbk) svbk = 1;
        address -= (uint16_t) WRAM_N_ADDRESS_START;
        return &(wram[svbk][address]);
    }
    else if (address <= ECHO_RAM_ADDRESS_END)
    {
        address -= (uint16_t) ECHO_RAM_OFFSET;
        memory[address] = memory[address];
        return &(memory[address]);
    }
    else if (address <= OAM_ADDRESS_END)
    {
        return &(memory[address]);
    }
    else if (address <= IO_REGISTERS_END)
    {
        return io_memory_read(address);
    }
    else if (address <= INTERRUPT_ENABLE_ADDRESS)
    {
        return &(memory[address]);
    }
    return &(memory[INTERRUPT_ENABLE_ADDRESS]);
}

void write_memory(uint16_t address, uint8_t value)
{ 
    if (address <= BANK_N_ADDRESS_END)
    {
        write_rom_memory(address, value);
    }
    else if (address <= VRAM_ADDRESS_END)
    {
        address -= (uint16_t) VRAM_ADDRESS_START;
        vram[((bool) memory[VBK])][address] = value;
    }
    else if (address <= EXT_RAM_ADDRESS_END)
    {
        write_rom_memory(address, value);
    }
    else if (address <= WRAM_ZERO_ADDRESS_END)
    {
        address -= (uint16_t) WRAM_ZERO_ADDRESS_START;
        wram[0][address] = value;
    }
    else if (address <= WRAM_N_ADDRESS_END)
    {
        uint8_t svbk = (memory[SVBK] & LOWER_3_MASK);
        if (!svbk) svbk = 1;
        address -= (uint16_t) WRAM_N_ADDRESS_START;
        wram[svbk][address] = value;
    }
    else if (address <= ECHO_RAM_ADDRESS_END)
    {
        memory[address] = value; 
        address -= (uint16_t) ECHO_RAM_OFFSET;
        memory[address - ECHO_RAM_OFFSET] = value;
    }
    else if (address <= OAM_ADDRESS_END)
    {
        memory[address] = value;
    }
    else if (address > NOT_USABLE_END && address <= IO_REGISTERS_END)
    {
        io_memory_write(address, value);
    }
    else if (address <= INTERRUPT_ENABLE_ADDRESS)
    {
        memory[address] = value;
    }
}