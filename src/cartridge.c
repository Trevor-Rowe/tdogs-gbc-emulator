#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cartridge.h"
#include "common.h"

/* CONSTANTS FOR READABILITY */

typedef enum
{
    TITLE_ADDRESS             = 0x0134,
    COLOR_MODE_ENABLE_ADDRESS = 0x0143,
    NEW_PUBLISHER_ADDRESS     = 0x0144,
    MBC_SCHEMA_ADDRESS        = 0x0147,
    DESTINATION_ADDRESS       = 0x014A,
    OLD_PUBLISHER_ADDRESS     = 0x014B,
    VERSION_ADDRESS           = 0x014C,
    CHECKSUM_ADDRESS          = 0x014D,
    ROM_SETTINGS_ADDRESS      = 0x0148,
    RAM_SETTINGS_ADDRESS      = 0x0149

} HeaderAddresses;

typedef enum
{
    RAM_ENABLE_ADDRESS      = 0x1FFF,
    ROM_BANK_SEL_L5_ADDRESS = 0x3FFF,
    RAM_BANK_SEL_ADDRESS    = 0x5FFF,
    SET_BANK_MODE_ADDRESS   = 0x7FFF, 
    ROM_EXECUTION_ADDRESS   = 0x0100

} RomAddresses;

typedef enum
{
    ROM_ONLY                       = 0x00,
    MBC1                           = 0x01,
    MBC1_RAM                       = 0x02,
    MBC1_RAM_BATTERY               = 0x03,
    MBC2                           = 0x05,
    MBC2_BATTERY                   = 0x06,
    MMM01                          = 0x0B,
    MMM01_RAM                      = 0x0C,
    MMM01_RAM_BATTERY              = 0x0D, 
    MBC3_TIMER_BATTERY             = 0x0F,
    MBC3                           = 0x11,
    MBC3_RAM                       = 0x12,
    MBC3_RAM_BATTERY               = 0x13,
    MBC5                           = 0x19,
    MBC5_RAM                       = 0x1A,
    MBC5_RAM_BATTERY               = 0x1B,
    MBC5_RUMBLE                    = 0x1C,
    MBC5_RUMBLE_RAM                = 0x1D,
    MBC5_RUMBLE_RAM_BATTERY        = 0x1E,
    MBC6                           = 0x20,
    MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22

} CartridgeTypes;

typedef enum
{
    ROM_BANK_MODE    = 0,
    RAM_BANK_MODE    = 1,
    DEFAULT_ROM_BANK = 1,
    DEFUALT_RAM_BANK = 1,
    RAM_ENABLED      = 0x0A,
    RAM_DISABLED     = 0x00,
    ROM_BANK_SIZE    = 0x4000,
    RAM_BANK_SIZE    = 8000,

} CartridgeSettings; 

typedef enum
{
    ROM_BANK_CODE_NONE = 0x00,
    ROM_BANK_CODE_4    = 0x01,
    ROM_BANK_CODE_8    = 0x02,
    ROM_BANK_CODE_16   = 0x03,
    ROM_BANK_CODE_32   = 0x04,
    ROM_BANK_CODE_64   = 0x05,
    ROM_BANK_CODE_128  = 0x06,
    ROM_BANK_CODE_256  = 0x07,
    ROM_BANK_CODE_512  = 0x08

} RomSettings;

typedef enum
{
    RAM_BANK_CODE_NONE   = 0x00,
    RAM_BANK_CODE_UNUSED = 0x01,
    RAM_BANK_CODE_1      = 0x02,
    RAM_BANK_CODE_4      = 0x03,
    RAM_BANK_CODE_16     = 0x04,
    RAM_BANK_CODE_8      = 0x05

} RamSettings;

/* STATE MANGEMENT */

typedef struct
{
    // ROM Memory info for banking purposes.
    unsigned long rom_size;
    uint16_t total_rom_banks;
    uint8_t  upper_bank;
    uint8_t  lower_bank;

} RomMemory; // (12 Bytes)

typedef struct
{
    // RAM Memory info for banking purposes.
    unsigned long ram_size;
    uint16_t total_ram_banks;
    uint16_t current_ram_bank;

} RamMemory; // (12 Bytes) 

typedef struct
{                       // Description             | [start - end)
    char title[15];     // Game Title              | 0x0134 - 0x0143
    uint8_t  cgb_flag;  // Enable Color Mode       | 0x0143 - 0x0144
    uint16_t nl_code;   // Game's publisher        | 0x0144 - 0x0146
    uint8_t cart_type;  // Mapping schema          | 0x0147 - 0x0148
    uint8_t dest_code;  // Destination code        | 0x014A - 0x014B
    uint8_t ol_code;    // Old license code        | 0x014B - 0x014C
    uint8_t version;    // Version number          | 0x014C - 0x014D
    uint8_t checksum;   // Header checksum         | 0x014D - 0x014E

} RomHeader; // (23 Bytes)

typedef struct
{
    long file_size;
    RomHeader header;
    RomMemory rom_memory;
    RamMemory ram_memory;
    int banking_mode; // 0-> ROM | 1-> RAM
    bool ram_enable;
    bool testing;
    uint8_t *content;

} Cartridge;  // (61 Bytes)

/* GLOBAL STATE POINTERS */

static Cartridge *cart;

/* STATIC (PRIVATE) HELPERS FUNCTIONS */

// Reads file and returns pointer to loaded content.
static uint8_t *get_rom_content(char *file_path, Cartridge *context) 
{
    // Open file with fopen() in "r"ead "b"inary mode.
    printf("Reading file at %s\n", file_path);
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("Failed to open file.");
        return NULL;
    }

    // Move to end of file to find its size.
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Allocate memory to a buffer.
    uint8_t *buffer = (uint8_t*) malloc(file_size);
    if (buffer == NULL) {
        perror("Memory allocation failed.");
        fclose(file);
        return NULL;
    }

    // Read value into buffer.
    size_t bytesRead = fread(buffer, 1, file_size, file);
    if (bytesRead != file_size) {
        perror("Error reading ROM file.");
        fclose(file);
        return NULL;
    }

    printf("Successfully loaded %ld bytes from %s\n", file_size, file_path);
    context->file_size = file_size;
    // Tidy up.
    fclose(file);

    return buffer;
}

// Loads cartridge title from ROM.
static void load_rom_title(Cartridge *context, uint8_t *contents)
{
    int size = 15; 
    for (int i = 0; i < size - 1; i++) 
    {
        context->header.title[i] =  contents[TITLE_ADDRESS + i];
    }
    context->header.title[size - 1] = '\0';
}

// Returns publisher name given 2 Byte code.
static const char *get_publisher_name(uint16_t hex_code) 
{
    switch (hex_code) 
    {
        case 0x3030: return "None";                 // '00'
        case 0x3031: return "Nintendo R&D1";        // '01'
        case 0x3038: return "Capcom";               // '08'
        case 0x3133: return "Electronic Arts";      // '13'
        case 0x3138: return "Hudson Soft";          // '18'
        case 0x3139: return "b-ai";                 // '19'
        case 0x3230: return "kss";                  // '20'
        case 0x3232: return "pow";                  // '22'
        case 0x3234: return "PCM Complete";         // '24'
        case 0x3235: return "San-X";                // '25'
        case 0x3238: return "Kemco Japan";          // '28'
        case 0x3239: return "seta";                 // '29'
        case 0x3330: return "Viacom";               // '30'
        case 0x3331: return "Nintendo";             // '31'
        case 0x3332: return "Bandai";               // '32'
        case 0x3333: return "Ocean/Acclaim";        // '33'
        case 0x3334: return "Konami";               // '34'
        case 0x3335: return "Hector";               // '35'
        case 0x3337: return "Taito";                // '37'
        case 0x3338: return "Hudson";               // '38'
        case 0x3339: return "Banpresto";            // '39'
        case 0x3431: return "Ubi Soft";             // '41'
        case 0x3432: return "Atlus";                // '42'
        case 0x3434: return "Malibu";               // '44'
        case 0x3436: return "Angel";                // '46'
        case 0x3437: return "Bullet-Proof";         // '47'
        case 0x3439: return "Irem";                 // '49'
        case 0x3530: return "Absolute";             // '50'
        case 0x3531: return "Acclaim";              // '51'
        case 0x3532: return "Activision";           // '52'
        case 0x3533: return "American Sammy";       // '53'
        case 0x3534: return "Konami";               // '54'
        case 0x3535: return "Hi Tech Entertainment";// '55'
        case 0x3536: return "LJN";                  // '56'
        case 0x3537: return "Matchbox";             // '57'
        case 0x3538: return "Mattel";               // '58'
        case 0x3539: return "Milton Bradley";       // '59'
        case 0x3630: return "Titus";                // '60'
        case 0x3631: return "Virgin";               // '61'
        case 0x3634: return "LucasArts";            // '64'
        case 0x3637: return "Ocean";                // '67'
        case 0x3639: return "Electronic Arts";      // '69'
        case 0x3730: return "Infogrames";           // '70'
        case 0x3731: return "Interplay";            // '71'
        case 0x3732: return "Broderbund";           // '72'
        case 0x3733: return "Sculptured";           // '73'
        case 0x3735: return "Sci";                  // '75'
        case 0x3738: return "THQ";                  // '78'
        case 0x3739: return "Accolade";             // '79'
        case 0x3830: return "Misawa";               // '80'
        case 0x3833: return "Lozc";                 // '83'
        case 0x3836: return "Tokuma Shoten Intermedia"; // '86'
        case 0x3837: return "Tsukuda Original";     // '87'
        case 0x3931: return "Chunsoft";             // '91'
        case 0x3932: return "Video System";         // '92'
        case 0x3933: return "Ocean/Acclaim";        // '93'
        case 0x3935: return "Varie";                // '95'
        case 0x3936: return "Yonezawa/s’pal";       // '96'
        case 0x3937: return "Kaneko";               // '97'
        case 0x3939: return "Pack in Soft";         // '99'
        case 0x4134: return "Konami (Yu-Gi-Oh!)";   // 'A4'
        default: return "Unknown Publisher";
    }
}

// Returns cartridge type (MBC, MBC1, ETC) given single byte code.
static const char *get_cartridge_type(uint8_t hex_code) 
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

// Loads ROM settings into cart based on provided single byte code.
static void load_rom_settings(uint8_t hex_code, Cartridge *context) 
{
   context->rom_memory.rom_size = 32000 * (1 << hex_code);
   switch (hex_code) 
   {
        case ROM_BANK_CODE_NONE:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 2; 
            break;
        case ROM_BANK_CODE_4:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 4; 
            break;
        case ROM_BANK_CODE_8:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 8; 
            break;
        case ROM_BANK_CODE_16:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 16;
            break;
        case ROM_BANK_CODE_32:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 32;
            break;
        case ROM_BANK_CODE_64:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 64;
            break;
        case ROM_BANK_CODE_128:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 128;
            break;
        case ROM_BANK_CODE_256:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 256;
            break;
        case ROM_BANK_CODE_512:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 512;
            break;
        default:
            context->rom_memory.lower_bank = 1;
            context->rom_memory.upper_bank = 0;
            context->rom_memory.total_rom_banks = 2;
            break;
    }
}

// Loads RAM settings into cart based on provided single byte code.
static void load_ram_settings(uint16_t hex_code, Cartridge *context)
{
    switch (hex_code)
    {
        case RAM_BANK_CODE_NONE:
            context->ram_memory.ram_size = 0;
            context->ram_memory.total_ram_banks = 0;
            context->ram_memory.current_ram_bank = 0;
            break;
        case RAM_BANK_CODE_UNUSED:
            context->ram_memory.ram_size = 0;
            context->ram_memory.total_ram_banks = 0;
            context->ram_memory.current_ram_bank = 0;
            break;
        case RAM_BANK_CODE_1:
            context->ram_memory.ram_size = 8000;
            context->ram_memory.total_ram_banks = 1;
            context->ram_memory.current_ram_bank = 1;
            break;
        case RAM_BANK_CODE_4:
            context->ram_memory.ram_size = 32000;
            context->ram_memory.total_ram_banks = 4;
            context->ram_memory.current_ram_bank = 1;
            break;
        case RAM_BANK_CODE_16:
            context->ram_memory.ram_size = 128000;
            context->ram_memory.total_ram_banks = 16;
            context->ram_memory.current_ram_bank = 1;
            break;
        case RAM_BANK_CODE_8:
            context->ram_memory.ram_size = 64000;
            context->ram_memory.total_ram_banks = 8;
            context->ram_memory.current_ram_bank = 1;
            break;
        default:
            context->ram_memory.ram_size = 0;
            context->ram_memory.total_ram_banks = 0;
            context->ram_memory.current_ram_bank = 0;
            break;
    }
}

// Constructs and returns value based on current upper and lower bits.
static uint8_t get_current_rom_bank()
{
    return (cart->rom_memory.upper_bank << 5) | (cart->rom_memory.lower_bank);
}

/* CLIENT (PUBLIC) FUNCTIONS */

int init_cartridge(char *file_path, bool testing)
{
    Cartridge *context = (Cartridge*) malloc(sizeof(Cartridge));
    uint8_t *content = get_rom_content(file_path, context);
    context->content = content;
    context->banking_mode = ROM_BANK_MODE;
    context->ram_enable   = true;
    context->testing      = true;
    if(!testing)
    { // Start collecting ROM header data.
        load_rom_title(context, content);
        load_rom_settings(content[ROM_SETTINGS_ADDRESS], context);
        load_ram_settings(content[RAM_SETTINGS_ADDRESS], context); 
        context->header.cgb_flag  = content[COLOR_MODE_ENABLE_ADDRESS];
        context->header.nl_code   = (
            content[NEW_PUBLISHER_ADDRESS]
            << 8 | // Shift right then bitwise OR.
            content[MBC_SCHEMA_ADDRESS]
        );
        context->header.cart_type = content[MBC_SCHEMA_ADDRESS];
        context->header.dest_code = content[DESTINATION_ADDRESS];
        context->header.ol_code   = content[OLD_PUBLISHER_ADDRESS];
        context->header.version   = content[VERSION_ADDRESS];
        context->header.checksum  = content[CHECKSUM_ADDRESS];
    }
    //Local tidy Up.
    cart = context;
    context = NULL;
    content = NULL;
}

void print_cartridge_info()
{
    printf("General Info:\n");
    printf("Title        = %s\n"    , cart->header.title);
    printf("CGB Flag     = %02X\n"  , cart->header.cgb_flag);
    printf("Publisher    = %s\n"    , get_publisher_name(cart->header.nl_code));
    printf("Cart Type    = %s\n"    , get_cartridge_type(cart->header.cart_type));
    printf("Version      = %d\n"    , cart->header.version);
    printf("Checksum     = %02X\n\n", cart->header.checksum);
    printf("ROM Info:\n");
    printf("Size         = %ld\n"   , cart->rom_memory.rom_size);
    printf("Current Bank = %d\n"    , get_current_rom_bank());
    printf("Total Banks  = %d\n\n"  , cart->rom_memory.total_rom_banks);
    printf("RAM Info:\n");
    printf("Size         = %ld\n"   , cart->ram_memory.ram_size);
    printf("Current Bank = %d\n"    , cart->ram_memory.current_ram_bank);
    printf("Total Banks  = %d\n\n"  , cart->ram_memory.total_ram_banks);
}

uint8_t read_rom_memory(uint16_t address)
{
    if (address < BANK_ZERO_ADDRESS)
    { // Bank 0 Read
        return cart->content[address];
    } 
    if (address < BANK_N_ADDRESS)
    { // Current Bank Read
        uint16_t offset = get_current_rom_bank() * ROM_BANK_SIZE;
        return cart->content[offset + (address - 0x4000)];
    }
    return 0xFF;
}

void write_rom_memory(uint16_t address, uint8_t value) 
{
    if     (address <= RAM_ENABLE_ADDRESS)
    { // RAM Enable
        switch(value)
        {
            case RAM_ENABLED:  
                cart->ram_enable = value & RAM_ENABLED == RAM_ENABLED;  
                break;
            case RAM_DISABLED: 
                cart->ram_enable = value & RAM_DISABLED == RAM_DISABLED;
                break;
        }
    } 
    else if(address <= ROM_BANK_SEL_L5_ADDRESS) 
    { // Select ROM Bank Number (Lower 5 Bits)
        cart->rom_memory.lower_bank = value & LOWER_5_MASK; 
    }
    else if(address <= RAM_BANK_SEL_ADDRESS)
    { // (RAM Bank Number) OR (Select Upper 2 Bits ROM) 
        if (cart->banking_mode)
        { // RAM Banking Mode
            if (value == 0x00)
            {
                cart->ram_memory.current_ram_bank = DEFUALT_RAM_BANK;
            }
            else if(value < cart->ram_memory.total_ram_banks) 
            {
                cart->ram_memory.current_ram_bank = value;
            }
        }
        else 
        { // Set Lower 2 Bits (Peform bitshift when calculating ROM Bank)
            cart->rom_memory.upper_bank = value & LOWER_2_MASK; 
        }
    }
    else if(address <= SET_BANK_MODE_ADDRESS) 
    { // Banking Mode Select
        switch(value)
        {
            case ROM_BANK_MODE: cart->banking_mode = ROM_BANK_MODE; break;
            case RAM_BANK_MODE: cart->banking_mode = RAM_BANK_MODE; break;
        }
    }
}

uint16_t get_rom_start()
{
    if(cart->testing) return 0x00;
    return ROM_EXECUTION_ADDRESS;
}

void tidy_cartridge()
{
    free(cart->content);
    cart->content = NULL;
    free(cart);
    cart = NULL;
}