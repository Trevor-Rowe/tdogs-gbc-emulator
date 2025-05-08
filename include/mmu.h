#ifndef MMU_H
#define MMU_H
#include <stdint.h>

typedef enum
{
    BANK_ZERO_ADDRESS_START  = (uint16_t) 0x0000,
    BANK_ZERO_ADDRESS_END    = (uint16_t) 0x3FFF,
    BANK_N_ADDRESS_START     = (uint16_t) 0x4000,
    BANK_N_ADDRESS_END       = (uint16_t) 0x7FFF,
    VRAM_ADDRESS_START       = (uint16_t) 0x8000,
    VRAM_ADDRESS_END         = (uint16_t) 0x9FFF,
    EXT_RAM_ADDRESS_START    = (uint16_t) 0xA000,
    EXT_RAM_ADDRESS_END      = (uint16_t) 0xBFFF,
    WRAM_ZERO_ADDRESS_START  = (uint16_t) 0xC000,
    WRAM_ZERO_ADDRESS_END    = (uint16_t) 0xCFFF,
    WRAM_N_ADDRESS_START     = (uint16_t) 0xD000,
    WRAM_N_ADDRESS_END       = (uint16_t) 0xDFFF,
    ECHO_RAM_ADDRESS_START   = (uint16_t) 0xE000,
    ECHO_RAM_ADDRESS_END     = (uint16_t) 0xFDFF,
    OAM_ADDRESS_START        = (uint16_t) 0xFE00,
    OAM_ADDRESS_END          = (uint16_t) 0xFE9F,
    NOT_USABLE_START         = (uint16_t) 0xFEA0,
    NOT_USABLE_END           = (uint16_t) 0xFEFF,
    IO_REGISTERS_START       = (uint16_t) 0xFF00,
    IO_REGISTERS_END         = (uint16_t) 0xFF7F,
    HIGH_RAM_ADDRESS_START   = (uint16_t) 0xFF80,
    HIGH_RAM_ADDRESS_END     = (uint16_t) 0xFFFE,
    INTERRUPT_ENABLE_ADDRESS = (uint16_t) 0xFFFF

} MemoryAddresses;

typedef enum 
{
    JOYP     = (uint16_t) 0xFF00, // Joypad
    SB       = (uint16_t) 0xFF01, // Serial transfer data
    SC       = (uint16_t) 0xFF02, // Serial transfer control
    DIV      = (uint16_t) 0xFF04, // Divider register
    TIMA     = (uint16_t) 0xFF05, // Timer counter
    TMA      = (uint16_t) 0xFF06, // Timer modulo
    TAC      = (uint16_t) 0xFF07, // Timer control
    IFR      = (uint16_t) 0xFF0F, // Interrupt flag
    NR10     = (uint16_t) 0xFF10, // Sound channel 1 sweep
    NR11     = (uint16_t) 0xFF11, // Sound channel 1 length and duty cycle 
    NR12     = (uint16_t) 0xFF12, // Sound channel 1 volume and envelope
    NR13     = (uint16_t) 0xFF13, // Sound channel 1 period low
    NR14     = (uint16_t) 0xFF14, // Sound channel 1 period high and control
    NR21     = (uint16_t) 0xFF16, // Sound channel 2 length and duty cycle
    NR22     = (uint16_t) 0xFF17, // Sound channel 2 volume and envelope
    NR23     = (uint16_t) 0xFF18, // Sound channel 2 period low
    NR24     = (uint16_t) 0xFF19, // Sound channel 2 period high and control
    NR30     = (uint16_t) 0xFF1A, // Sound channel 3 DAC enable
    NR31     = (uint16_t) 0xFF1B, // Sound channel 3 length timer
    NR32     = (uint16_t) 0xFF1C, // Sound channel 3 output level
    NR33     = (uint16_t) 0xFF1D, // Sound channel 3 period low
    NR34     = (uint16_t) 0xFF1E, // Sound channel 3 period high and control
    NR41     = (uint16_t) 0xFF20, // Sound channel 4 legnth timer
    NR42     = (uint16_t) 0xFF21, // Sound channel 4 volume and envelope
    NR43     = (uint16_t) 0xFF22, // Sound channel 4 frequency and randomness
    NR44     = (uint16_t) 0xFF23, // Sound chennel 4 control
    NR50     = (uint16_t) 0xFF24, // Master volume and VIN panning
    NR51     = (uint16_t) 0xFF25, // Sound panning
    NR52     = (uint16_t) 0xFF26, // Sound on/off
    WR_START = (uint16_t) 0xFF30, 
    WR_END   = (uint16_t) 0xFF3F, // Storage for one SC waveform 
    LCDC     = (uint16_t) 0xFF40, // LCD control
    STAT     = (uint16_t) 0xFF41, // LCD status
    SCY      = (uint16_t) 0xFF42, // Viewport Y posiiton
    SCX      = (uint16_t) 0xFF43, // Viewport X position
    LY       = (uint16_t) 0xFF44, // LCD Y coordinate
    LYC      = (uint16_t) 0xFF45, // LY compare
    DMA      = (uint16_t) 0xFF46, // OAM DMA source address and start
    BGP      = (uint16_t) 0xFF47, // BG palette data
    OBP0     = (uint16_t) 0xFF48, // OBJ pallete 0 data
    OBP1     = (uint16_t) 0xFF49, // OBJ pallete 1 data
    WY       = (uint16_t) 0xFF4A, // Window Y position
    WX       = (uint16_t) 0xFF4B, // Windows X position plus 7
    KEY1     = (uint16_t) 0xFF4D, // prepare speed switch                     (CGB)
    VBK      = (uint16_t) 0xFF4F, // VRAM bank                                (CGB)
    BIOS     = (uint16_t) 0xFF50, // BIOS -> Non-Zero Disables BOOT ROM
    HDMA1    = (uint16_t) 0xFF51, // VRAM DMA source high                     (CGB)
    HDMA2    = (uint16_t) 0xFF52, // VRAM DMA source low                      (CGB)
    HDMA3    = (uint16_t) 0xFF53, // VRAM DMA destination high                (CGB)
    HDMA4    = (uint16_t) 0xFF54, // VRAM DMA destination low                 (CGB)
    HDMA5    = (uint16_t) 0xFF55, // VRAM DMA length/mode/start               (CGB)
    RP       = (uint16_t) 0xFF56, // Infrared communication port              (CGB)
    BCPS     = (uint16_t) 0xFF68, // Background color pallette specification  (CGB)
    BCPD     = (uint16_t) 0xFF69, // Background color pallette data           (CGB)
    OCPS     = (uint16_t) 0xFF6A, // Object color pallette specification      (CGB)
    OCPD     = (uint16_t) 0xFF6B, // Object color pallette data               (CGB)
    OPRI     = (uint16_t) 0xFF6C, // Object priority mode                     (CGB)
    SVBK     = (uint16_t) 0xFF70, // WRAM bank                                (CGB)
    PCM12    = (uint16_t) 0xFF76, // Audio digital outputs 1 & 2              (CGB)
    PCM34    = (uint16_t) 0xFF77, // Audio digital outputs 3 & 4              (CGB)
    IER      = (uint16_t) 0xFFFF, // Interrupt enable                         

} HardwareRegisters;

typedef enum
{
    VBLANK_VECTOR = (uint16_t) 0x0040,
    LCD_VECTOR    = (uint16_t) 0x0048,
    TIMER_VECTOR  = (uint16_t) 0x0050,
    SERIAL_VECTOR = (uint16_t) 0x0058,
    JOYPAD_VECTOR = (uint16_t) 0x0060
    
} InterruptVector;

void init_memory();

void tidy_memory();

uint8_t read_memory(uint16_t address);

void write_memory(uint16_t address, uint8_t value);

void check_dma();

bool dma_active();

uint8_t read_joypad();

uint8_t read_vram(uint8_t bank, uint16_t address);

uint8_t read_cram(bool is_obj, uint8_t palette_index, uint8_t color_id, uint8_t index);

uint8_t *get_memory();

uint8_t *get_memory_pointer(uint16_t address);

void print_vram(uint16_t start, uint16_t end, bool bank);

uint8_t io_memory_read(uint16_t address);

void io_memory_write(uint16_t address, uint8_t value);

#endif