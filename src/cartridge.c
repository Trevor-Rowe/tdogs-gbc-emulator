#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cartridge.h>

typedef struct
{
    long file_size;
    rom_header header;
    rom_memory rom_memory;
    ram_memory ram_memory;
    uint8_t *content;

} cartridge_context;

static cartridge_context *cart_context;

/* Local Helper Functions */

// Reads file and returns pointer to loaded content.
static uint8_t *get_rom_content(char *file_path, cartridge_context *context) 
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

    printf("Successfully loaded %ld from %s\n", file_size, file_path);
    context->file_size = file_size;
    // Tidy up.
    fclose(file);

    return buffer;
}

// Loads context->header.title char array.
static void load_rom_title(cartridge_context *context, uint8_t *contents)
{
    int size = 15; // 0x0134 - 0x0143
    for (int i = 0; i < size - 1; i++) 
    {
        context->header.title[i] =  contents[0x0134 + i];
    }
    context->header.title[size - 1] = '\0';
}

static const char* get_publisher_name(uint16_t hex_code) 
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

const char* get_cartridge_type(uint8_t hex_code) 
{
    switch (hex_code) 
    {
        case 0x00: return "ROM ONLY";
        case 0x01: return "MBC1";
        case 0x02: return "MBC1+RAM";
        case 0x03: return "MBC1+RAM+BATTERY";
        case 0x05: return "MBC2";
        case 0x06: return "MBC2+BATTERY";
        case 0x08: return "ROM+RAM";
        case 0x09: return "ROM+RAM+BATTERY";
        case 0x0B: return "MMM01";
        case 0x0C: return "MMM01+RAM";
        case 0x0D: return "MMM01+RAM+BATTERY";
        case 0x0F: return "MBC3+TIMER+BATTERY";
        case 0x10: return "MBC3+TIMER+RAM+BATTERY";
        case 0x11: return "MBC3";
        case 0x12: return "MBC3+RAM";
        case 0x13: return "MBC3+RAM+BATTERY";
        case 0x19: return "MBC5";
        case 0x1A: return "MBC5+RAM";
        case 0x1B: return "MBC5+RAM+BATTERY";
        case 0x1C: return "MBC5+RUMBLE";
        case 0x1D: return "MBC5+RUMBLE+RAM";
        case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
        case 0x20: return "MBC6";
        case 0x22: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        case 0xFC: return "POCKET CAMERA";
        case 0xFD: return "BANDAI TAMA5";
        case 0xFE: return "HuC3";
        case 0xFF: return "HuC1+RAM+BATTERY";
        default: return "Unknown Cartridge Type";
    }
}

// Loads ROM settings into context.
static void load_rom_settings(uint8_t hex_code, cartridge_context *context) 
{
   context->rom_memory.rom_size = 32000 * (1 << hex_code);
   switch (hex_code) 
   {
        case 0x00:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 2; 
            break;
        case 0x01:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 4; 
            break;
        case 0x02:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 8; 
            break;
        case 0x03:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 16;
            break;
        case 0x04:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 32;
            break;
        case 0x05:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 64;
            break;
        case 0x06:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 128;
            break;
        case 0x07:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 256;
            break;
        case 0x08:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 512;
            break;
        default:
            context->rom_memory.current_rom_bank = 1;
            context->rom_memory.total_rom_banks = 2;
            break;
    }
}

static void load_ram_settings(uint16_t hex_code, cartridge_context *context)
{
    switch (hex_code)
    {
        case 0x00:
            context->ram_memory.ram_size = 0;
            context->ram_memory.total_ram_banks = 0;
            context->ram_memory.current_ram_bank = 0;
            break;
        case 0x01:
            context->ram_memory.ram_size = 0;
            context->ram_memory.total_ram_banks = 0;
            context->ram_memory.current_ram_bank = 0;
            break;
        case 0x02:
            context->ram_memory.ram_size = 8000;
            context->ram_memory.total_ram_banks = 1;
            context->ram_memory.current_ram_bank = 1;
            break;
        case 0x03:
            context->ram_memory.ram_size = 32000;
            context->ram_memory.total_ram_banks = 4;
            context->ram_memory.current_ram_bank = 1;
            break;
        case 0x04:
            context->ram_memory.ram_size = 128000;
            context->ram_memory.total_ram_banks = 16;
            context->ram_memory.current_ram_bank = 1;
            break;
        case 0x05:
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

/* Implemented .h functions*/

int init_cartridge(char *file_path)
{
    cartridge_context *context = (cartridge_context*) malloc(sizeof(cartridge_context));
    uint8_t *content = get_rom_content(file_path, context);
    context->content = content;
    // Start collecting ROM header data.
    load_rom_title(context, content);
    load_rom_settings(content[0x0148], context);
    load_ram_settings(content[0x0149], context); 
    context->header.cgb_flag  = content[0x0143];
    context->header.nl_code   = (content[0x0144] << 8 | content[0x0145]);
    context->header.cart_type = content[0x0147];
    context->header.dest_code = content[0x014A];
    context->header.ol_code   = content[0x014B];
    context->header.version   = content[0x014C];
    context->header.checksum  = content[0x014D];
    //Tidy Up. TODO: Create a function that does this on exit, as these are needed for execution.
    cart_context = context;
    context, content = NULL; // Disarm local pointers.
}

void print_cartridge_info()
{
    // Print header data.
    printf("Title     = %s\n",   cart_context->header.title);
    printf("CGB Flag  = %02X\n", cart_context->header.cgb_flag);
    printf("Publisher = %s\n",   get_publisher_name(cart_context->header.nl_code));
    printf("Cart Type = %s\n",   get_cartridge_type(cart_context->header.cart_type));
    printf("Version   = %d\n",   cart_context->header.version);
    printf("Checksum  = %02X\n", cart_context->header.checksum);
}

// Free memory from global/context.
void tidy_cartridge()
{
    // Free content
    free(cart_context->content);
    cart_context->content = NULL;
    // Free context
    free(cart_context);
    cart_context = NULL;
}