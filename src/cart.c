#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "mmu.h"
#include "cart.h"
#include "common.h"
#include "logger.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)
#define DEFAULT_BANK 1
#define MEGABYTE 0x100000
#define KB_32    0x8000
#define DMG_BIOS "../roms/bios/dmg.bin"
#define CGB_BIOS "../roms/bios/cgb.bin"

/* CONSTANTS FOR READABILITY */

typedef enum
{
    TITLE_ADDRESS             = (uint16_t) 0x0134,
    COLOR_MODE_ENABLE_ADDRESS = (uint16_t) 0x0143,
    NEW_PUBLISHER_ADDRESS     = (uint16_t) 0x0144,
    MBC_SCHEMA_ADDRESS        = (uint16_t) 0x0147,
    DESTINATION_ADDRESS       = (uint16_t) 0x014A,
    OLD_PUBLISHER_ADDRESS     = (uint16_t) 0x014B,
    VERSION_ADDRESS           = (uint16_t) 0x014C,
    CHECKSUM_ADDRESS          = (uint16_t) 0x014D,
    ROM_SETTINGS_ADDRESS      = (uint16_t) 0x0148,
    RAM_SETTINGS_ADDRESS      = (uint16_t) 0x0149

} HeaderAddress;

typedef enum
{
    RAM_ENABLE_ADDRESS      = (uint16_t) 0x1FFF,
    ROM_BANK_SEL_L5_ADDRESS = (uint16_t) 0x3FFF,
    RAM_BANK_SEL_ADDRESS    = (uint16_t) 0x5FFF,
    SET_BANK_MODE_ADDRESS   = (uint16_t) 0x7FFF, 
    ROM_EXECUTION_ADDRESS   = (uint16_t) 0x0100

} RomAddresses;

typedef enum
{
    ROM_BANK_SIZE = (uint16_t) 0x4000,
    RAM_BANK_SIZE = (uint16_t) 0x2000,

    MBC1_RAM_BANK_MODE = (uint8_t) 0x00,
    MBC1_ROM_BANK_MODE = (uint8_t) 0x01

} MbcBankConstant;

typedef enum
{
    ROM_ONLY                       = (uint8_t) 0x00,
    MBC1                           = (uint8_t) 0x01,
    MBC1_RAM                       = (uint8_t) 0x02,
    MBC1_RAM_BATTERY               = (uint8_t) 0x03,
    MBC2                           = (uint8_t) 0x05,
    MBC2_BATTERY                   = (uint8_t) 0x06,
    MMM01                          = (uint8_t) 0x0B,
    MMM01_RAM                      = (uint8_t) 0x0C,
    MMM01_RAM_BATTERY              = (uint8_t) 0x0D, 
    MBC3_TIMER_BATTERY             = (uint8_t) 0x0F,
    MBC3                           = (uint8_t) 0x11,
    MBC3_RAM                       = (uint8_t) 0x12,
    MBC3_RAM_BATTERY               = (uint8_t) 0x13,
    MBC5                           = (uint8_t) 0x19,
    MBC5_RAM                       = (uint8_t) 0x1A,
    MBC5_RAM_BATTERY               = (uint8_t) 0x1B,
    MBC5_RUMBLE                    = (uint8_t) 0x1C,
    MBC5_RUMBLE_RAM                = (uint8_t) 0x1D,
    MBC5_RUMBLE_RAM_BATTERY        = (uint8_t) 0x1E,
    MBC6                           = (uint8_t) 0x20,
    MBC7_SENSOR_RUMBLE_RAM_BATTERY = (uint8_t) 0x22

} CartridgeCode;

/* STATE MANGEMENT */

typedef struct
{ 
    char     title[15]; // Game Title        | 0x0134 - 0x0143
    uint8_t   cgb_code; // Enable Color Mode | 0x0143 - 0x0144
    uint16_t   nl_code; // Game's publisher  | 0x0144 - 0x0146
    uint8_t  cart_code; // Mapping schema    | 0x0147 - 0x0148
    uint8_t  dest_code; // Destination code  | 0x014A - 0x014B
    uint8_t    ol_code; // Old license code  | 0x014B - 0x014C
    uint8_t    version; // Version number    | 0x014C - 0x014D
    uint8_t   checksum; // Header checksum   | 0x014D - 0x014E

} Header;

typedef struct
{
    unsigned long    file_size;
    CartridgeCode    cart_code;
    
    bool           ram_enabled;
    uint8_t          bank_mode;
    uint8_t       rom_bank_sel;
    uint8_t         upper_bits;

    uint8_t           ram_code;
    uint8_t  ram_bank_quantity;
    
    uint8_t           rom_code;
    uint16_t rom_bank_quantity;
    uint8_t      rom_bank_mask;

} Cartridge;

/* GLOBAL STATE POINTERS */

static Cartridge   *cart;
static Header    *header;
static uint8_t      *rom;
static uint8_t *dmg_bios;
static uint8_t *cgb_bios;
static uint8_t     *bios;

/* STATIC (PRIVATE) HELPERS FUNCTIONS */

bool is_gbc() // $80 or $C0
{
    return ((header->cgb_code == 0x80) || (header->cgb_code == 0xC0));
}

static uint8_t *get_rom_content(char *file_path, Cartridge *cart) // Reads file and returns pointer to loaded content.
{
    // Open file with fopen() in "r"ead "b"inary mode.
    //LOG_MESSAGE(INFO, "Reading file at %s", file_path);
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        LOG_MESSAGE(ERROR, "Failed to open file.");
        return NULL;
    }

    // Move to end of file to find its size.
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Allocate memory to a buffer.
    uint8_t *buffer = (uint8_t*) malloc(file_size);
    if (buffer == NULL) {
       // LOG_MESSAGE(ERROR, "Memory allocation failed.");
        fclose(file);
        return NULL;
    }

    // Read value into buffer.
    size_t bytesRead = fread(buffer, 1, file_size, file);
    if (bytesRead != file_size) {
       // LOG_MESSAGE(ERROR, "Error reading ROM file.");
        fclose(file);
        return NULL;
    }

   // LOG_MESSAGE(INFO, "Successfully loaded %ld bytes from %s.", file_size, file_path);
    cart->file_size = file_size;
    // Tidy up.
    fclose(file);

    return buffer;
}

static void load_rom_title(Header *header, uint8_t *rom)          // Loads cartridge title from ROM.
{
    int size = 15; 
    for (int i = 0; i < size - 1; i++) 
    {
        header->title[i] =  rom[TITLE_ADDRESS + i];
    }
    header->title[size - 1] = '\0';
}

static void load_header(Header *header, uint8_t *rom)             // Load header data into struct.
{
    header->cart_code = rom[MBC_SCHEMA_ADDRESS];
    header-> cgb_code = rom[COLOR_MODE_ENABLE_ADDRESS];
    header-> checksum = rom[CHECKSUM_ADDRESS];
    header->dest_code = rom[DESTINATION_ADDRESS];
    header->  nl_code = rom[NEW_PUBLISHER_ADDRESS];
    header->  ol_code = rom[OLD_PUBLISHER_ADDRESS];
    header->  version = rom[VERSION_ADDRESS];
    load_rom_title(header, rom);
}

static const char *get_cartridge_name(uint8_t hex_code)           // Returns cartridge type label (MBC, MBC1, ETC) given single byte code.
{
    switch (hex_code) 
    {
        case ROM_ONLY:                       return "ROM ONLY";
        case MBC1:                           return "MBC1";
        case MBC1_RAM:                       return "MBC1+RAM";
        case MBC1_RAM_BATTERY:               return "MBC1+RAM+BATTERY";
        case MBC2:                           return "MBC2";
        case MBC2_BATTERY:                   return "MBC2+BATTERY";
        case MMM01:                          return "MMM01";
        case MMM01_RAM:                      return "MMM01+RAM";
        case MMM01_RAM_BATTERY:              return "MMM01+RAM+BATTERY";
        case MBC3_TIMER_BATTERY:             return "MBC3+TIMER+BATTERY";
        case MBC3:                           return "MBC3";
        case MBC5:                           return "MBC5";
        case MBC5_RAM:                       return "MBC5+RAM";
        case MBC5_RAM_BATTERY:               return "MBC5+RAM+BATTERY";
        case MBC5_RUMBLE:                    return "MBC5+RUMBLE";
        case MBC5_RUMBLE_RAM:                return "MBC5+RUMBLE+RAM";
        case MBC5_RUMBLE_RAM_BATTERY:        return "MBC5+RUMBLE+RAM+BATTERY";
        case MBC6:                           return "MBC6";
        case MBC7_SENSOR_RUMBLE_RAM_BATTERY: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        default:                             return "Unknown Cartridge Type";
    }
}

static void set_banking_mode(Cartridge *cart, MbcBankConstant mode)
{
    cart->bank_mode = mode;
}

static void set_bank_selection_register(Cartridge *cart, uint8_t bank, uint8_t mask)
{
    bank &= mask;

    if (bank > cart->rom_bank_quantity) // Upper bound protection.
    {
        bank &= cart->rom_bank_mask;
    }
    
    if (bank == 0)                      // Lower bound protection.
    {
        bank = 1;
    }

    cart->rom_bank_sel = bank;
}

static bool is_ram_accessible(Cartridge *cart, uint16_t address)
{
    bool accessible = 
    (
        (address >= EXT_RAM_ADDRESS_START) && 
        (address <= EXT_RAM_ADDRESS_END)   &&
        (cart->ram_enabled)
    );
    return accessible;
}

static void set_upper_bits(Cartridge *cart, uint8_t bits)
{
    bits &= LOWER_2_MASK;
    cart->upper_bits = bits;
}

static uint8_t get_bank_mask(uint8_t quantity)
{
    uint8_t mask = 0x00;
    while (quantity >= 2)
    {
        quantity = quantity / 2;
        mask     = (mask << 1) + 1;
    }
    return mask;
}

static void encode_rom_settings(Cartridge *cart)
{
    cart->rom_bank_sel = DEFAULT_BANK;
    switch(cart->rom_code)
    {
        case 0x00: cart->rom_bank_quantity = (uint16_t)   2; break;
        case 0x01: cart->rom_bank_quantity = (uint16_t)   4; break;
        case 0x02: cart->rom_bank_quantity = (uint16_t)   8; break;
        case 0x03: cart->rom_bank_quantity = (uint16_t)  16; break;
        case 0x04: cart->rom_bank_quantity = (uint16_t)  32; break;
        case 0x05: cart->rom_bank_quantity = (uint16_t)  64; break;
        case 0x06: cart->rom_bank_quantity = (uint16_t) 128; break;
        case 0x07: cart->rom_bank_quantity = (uint16_t) 256; break;
        case 0x08: cart->rom_bank_quantity = (uint16_t) 512; break;
        default:   cart->rom_bank_quantity = (uint16_t)   2; break; 
    }
    cart->rom_bank_mask = get_bank_mask(cart->rom_bank_quantity);
}

static void encode_ram_settings(Cartridge *cart, Header *header)
{
    cart->ram_enabled = 
    (
        (header->cart_code ==                       MBC1_RAM) ||
        (header->cart_code ==               MBC1_RAM_BATTERY) ||
        (header->cart_code ==                      MMM01_RAM) ||
        (header->cart_code ==              MMM01_RAM_BATTERY) ||
        (header->cart_code ==                       MBC3_RAM) ||
        (header->cart_code ==               MBC3_RAM_BATTERY) ||
        (header->cart_code ==                       MBC5_RAM) ||
        (header->cart_code ==               MBC5_RAM_BATTERY) ||
        (header->cart_code ==                MBC5_RUMBLE_RAM) ||
        (header->cart_code ==        MBC5_RUMBLE_RAM_BATTERY) ||
        (header->cart_code == MBC7_SENSOR_RUMBLE_RAM_BATTERY) 
    );
    if (!cart->ram_enabled) return;

    cart->rom_bank_sel = DEFAULT_BANK;
    switch (cart->ram_code)
    {
        case 0x02: cart->ram_bank_quantity = (uint8_t)  1; break;
        case 0x03: cart->ram_bank_quantity = (uint8_t)  4; break;
        case 0x04: cart->ram_bank_quantity = (uint8_t) 16; break;
        case 0x05: cart->ram_bank_quantity = (uint8_t)  8; break;
    }
}

/* CLIENT (PUBLIC) FUNCTIONS */

typedef uint8_t (*MbcReadHandler)(Cartridge*, uint16_t); /* CARTRIDGE MEMORY READING */

static uint8_t rom_only_read(Cartridge *cart, uint16_t address)
{
    if (address <= BANK_N_ADDRESS_END)
    {
        return rom[address];
    }
}

static uint8_t mbc1_read(Cartridge *cart, uint16_t address)
{
    if ((address >= 0x0000) && (address <= 0x3FFF)) // Static Bank
    {
        return rom[address];
    }

    if ((address >= 0x4000) && (address <= 0x7FFF)) // Dynamic Bank
    {
        uint8_t rom_bank = (cart->upper_bits << 5) + cart->rom_bank_sel;
        uint16_t  offset = rom_bank * ROM_BANK_SIZE;
        return rom[address + offset];
    }
}
static uint8_t mbc1_ram_read(Cartridge *cart, uint16_t address)
{
    if ((address >= 0x0000) && (address <= 0x7FFF)) // ROM Read
    {
        return mbc1_read(cart, address);
    }

    if (is_ram_accessible(cart, address) && (cart->bank_mode == MBC1_ROM_BANK_MODE)) // RAM Read
    {
        return rom[address];
    }

    if (is_ram_accessible(cart, address) && (cart->bank_mode == MBC1_RAM_BANK_MODE)) // RAM Read
    {
        uint16_t      offset = (cart->upper_bits * RAM_BANK_SIZE);
        uint16_t ext_address = (address + offset);
        return rom[ext_address];        
    }
}


static uint8_t mbc2_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc2_battery_read(Cartridge *cart, uint16_t address)
{

}

static uint8_t mmm01_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mmm01_ram_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mmm01_rb_read(Cartridge *cart, uint16_t address)
{

}

static uint8_t mbc3_tb_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc3_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc3_ram_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc3_rb_read(Cartridge *cart, uint16_t address)
{

}

static uint8_t mbc5_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc5_ram_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc5_rb_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc5_rumble_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc5_rr_read(Cartridge *cart, uint16_t address)
{

}
static uint8_t mbc5_rrb_read(Cartridge *cart, uint16_t address)
{

}

static uint8_t mbc6_read(Cartridge *cart, uint16_t address)
{

}

static uint8_t mbc7_read(Cartridge *cart, uint16_t address)
{

}

const MbcReadHandler mbc_read_table[0x23] = 
{
    [ROM_ONLY]                       =     rom_only_read,
    [MBC1]                           =         mbc1_read,
    [MBC1_RAM]                       =     mbc1_ram_read,
    [MBC1_RAM_BATTERY]               =     mbc1_ram_read,
    [MBC2]                           =         mbc2_read,
    [MBC2_BATTERY]                   = mbc2_battery_read,
    [MMM01]                          =        mmm01_read,
    [MMM01_RAM]                      =    mmm01_ram_read,
    [MMM01_RAM_BATTERY]              =     mmm01_rb_read,
    [MBC3_TIMER_BATTERY]             =      mbc3_tb_read,
    [MBC3]                           =         mbc3_read,
    [MBC3_RAM]                       =     mbc3_ram_read,
    [MBC3_RAM_BATTERY]               =      mbc3_rb_read,
    [MBC5]                           =         mbc5_read,
    [MBC5_RAM]                       =     mbc5_ram_read,
    [MBC5_RAM_BATTERY]               =      mbc5_rb_read,
    [MBC5_RUMBLE]                    =  mbc5_rumble_read, 
    [MBC5_RUMBLE_RAM]                =      mbc5_rr_read,
    [MBC5_RUMBLE_RAM_BATTERY]        =     mbc5_rrb_read,
    [MBC6]                           =         mbc6_read,
    [MBC7_SENSOR_RUMBLE_RAM_BATTERY] =         mbc7_read,
};

uint8_t read_rom_memory(uint16_t address) // Public API
{   
    if ((*bios) == 1)
    {
        return mbc_read_table[cart->cart_code](cart, address);
    }

    if (((*bios) == 0) && (is_gbc()) && ((address < 0x0100) || (address >= 0x0200)))
    {
        return cgb_bios[address];
    }

    if (((*bios) ==  0) && (!is_gbc()) && (address < 0x0100))
    {
        return dmg_bios[address];
    }
    
    return mbc_read_table[cart->cart_code](cart, address);
}

typedef void (*MbcWriteHandler)(Cartridge*, uint16_t, uint8_t); /* CARTRIDGE MEMORY WRITING */

static void rom_only_write(Cartridge *cart, uint16_t address, uint8_t value)
{
    return; // Read-Only!
}

static void mbc1_write(Cartridge *cart, uint16_t address, uint8_t value)
{
    if ((address >= 0x0000) && (address <= 0x1FFF)) // RAM Enable
    {
        cart->ram_enabled = ((value & LOWER_4_MASK) == 0x0A);
        return;
    }

    if ((address >= 0x2000) && (address <= 0x3FFF)) // ROM Bank Number
    {
        set_bank_selection_register(cart, value, LOWER_5_MASK);
        return;
    }

    if ((address >= 0x4000) && (address <= 0x5FFF)) // RAM Bank Number OR Upper Bits ROM Bank Number
    {
        set_upper_bits(cart, value);
        return;
    }

    if ((address >= 0x6000) && (address <= 0x7FFF)) // Banking Mode Select
    {
        (value == 1) ? 
        set_banking_mode(cart, MBC1_ROM_BANK_MODE): 
        set_banking_mode(cart, MBC1_RAM_BANK_MODE);
        return;
    }
}
static void mbc1_ram_write(Cartridge *cart, uint16_t address, uint8_t value)
{
    if ((address >= 0x0000) && (address <= 0x7FFF))
    {
        mbc1_write(cart, address, value);
        return;
    }

    if (is_ram_accessible(cart, address) && (cart->bank_mode == MBC1_ROM_BANK_MODE))
    {
        rom[address] = value;
        return;
    }

    if (is_ram_accessible(cart, address) && (cart->bank_mode == MBC1_RAM_BANK_MODE))
    {
        uint16_t          offset = (cart->upper_bits * RAM_BANK_SIZE);
        uint16_t ext_ram_address = (address + offset);
        rom[ext_ram_address] = value;
        return;
    }
}

static void mbc2_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}

static void mmm01_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mmm01_ram_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}

static void mbc3_tb_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc3_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc3_ram_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc3_rb_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}

static void mbc5_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc5_ram_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc5_rb_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc5_rumble_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc5_rr_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}
static void mbc5_rrb_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}

static void mbc6_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}

static void mbc7_write(Cartridge *cart, uint16_t address, uint8_t value)
{

}

const MbcWriteHandler mbc_write_table[0x23] = 
{
    [ROM_ONLY]                       =     rom_only_write,
    [MBC1]                           =         mbc1_write,
    [MBC1_RAM]                       =     mbc1_ram_write,
    [MBC1_RAM_BATTERY]               =     mbc1_ram_write,
    [MBC2]                           =         mbc2_write,
    [MBC2_BATTERY]                   =         mbc2_write,
    [MMM01]                          =        mmm01_write,
    [MMM01_RAM]                      =    mmm01_ram_write,
    [MMM01_RAM_BATTERY]              =    mmm01_ram_write,
    [MBC3_TIMER_BATTERY]             =      mbc3_tb_write,
    [MBC3]                           =         mbc3_write,
    [MBC3_RAM]                       =     mbc3_ram_write,
    [MBC3_RAM_BATTERY]               =      mbc3_rb_write,
    [MBC5]                           =         mbc5_write,
    [MBC5_RAM]                       =     mbc5_ram_write,
    [MBC5_RAM_BATTERY]               =      mbc5_rb_write,
    [MBC5_RUMBLE]                    =  mbc5_rumble_write, 
    [MBC5_RUMBLE_RAM]                =      mbc5_rr_write,
    [MBC5_RUMBLE_RAM_BATTERY]        =     mbc5_rrb_write,
    [MBC6]                           =         mbc6_write,
    [MBC7_SENSOR_RUMBLE_RAM_BATTERY] =         mbc7_write,
};

void write_rom_memory(uint16_t address, uint8_t value) // Public API
{
    if ((*bios) == 0) return;
    mbc_write_table[cart->cart_code](cart, address, value);
}

int init_cartridge(char *file_path, uint16_t entry)
{
    header   = (Header*) malloc(sizeof(Header));
    cart     = (Cartridge*) malloc(sizeof(Cartridge));
    dmg_bios = get_rom_content(DMG_BIOS,  cart);
    cgb_bios = get_rom_content(CGB_BIOS,  cart);
    rom      = get_rom_content(file_path, cart);
    bios = get_memory_pointer(BIOS);
    load_header(header, rom);
    encode_rom_settings(cart);
    encode_ram_settings(cart, header);
}

void tidy_cartridge()
{
    free(header);     header = NULL;
    free(cart);         cart = NULL;
    free(dmg_bios); dmg_bios = NULL;
    free(cgb_bios); cgb_bios = NULL;
    free(rom);           rom = NULL;
}