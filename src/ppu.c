#include <stdint.h>
#include <stdlib.h>
#include "cart.h"
#include "common.h"
#include "cpu.h"
#include "emulator.h"
#include "logger.h"
#include "mmu.h"
#include "ppu.h"
#include "util.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

typedef enum
{
    WHITE      = 0xFFE0F8D0,
    LIGHT_GRAY = 0xFF88C070,
    DARK_GRAY  = 0xFF346856,
    BLACK      = 0xFF081820

} DmgColors;

typedef struct
{
    uint16_t     oam;
    uint16_t drawing;
    uint16_t h_blank;
    uint16_t v_blank;

} PpuDelayEmulation;

static uint32_t          *lcd;
static Queue        *bgw_fifo;
static Queue        *obj_fifo;
static Queue        *obj_scan;
static GbcPixel *pixel_schema;
static uint8_t        *memory;

/* CREATE AND DESTROY WINDOW */
bool init_graphics()
{
    bgw_fifo     = init_queue(GBC_WIDTH);
    obj_fifo     = init_queue(GBC_WIDTH);
    obj_scan     = init_queue(OBJ_PER_LINE);
    pixel_schema = (GbcPixel*) malloc(sizeof(GbcPixel));
    lcd          = (uint32_t*) malloc(GBC_HEIGHT * GBC_WIDTH * sizeof(uint32_t));
    memory       = get_memory();
}

void tidy_graphics()
{
    free(lcd);
    lcd = NULL;
    free(pixel_schema);
    pixel_schema = NULL;
    tidy_queue(bgw_fifo);
    tidy_queue(obj_fifo);
    tidy_queue(obj_scan);
}

static void oam_scan(uint8_t ly)
{ // Scans for visible object on given scanline. (MODE 2)
   uint16_t current_entry = OAM_ADDRESS_START;
   bool stacked = (bool) (memory[LCDC] & BIT_1_MASK);
   reset_queue(obj_scan);
   while(current_entry < OAM_ADDRESS_END)
   {
        uint8_t      y_obj = memory[current_entry];
        uint8_t      x_obj = memory[current_entry + 1];
        uint8_t tile_index = memory[current_entry + 2];
        uint8_t  tile_attr = memory[current_entry + 3];
        bool visible = (0 < (ly - y_obj + 16)) && ((ly - y_obj + 16) < (8 + (8 * stacked)));
        if(visible) 
        {
            pixel_schema->oam_address  = current_entry;
            pixel_schema->       x_obj = x_obj;
            pixel_schema->       y_obj = y_obj;
            pixel_schema->  tile_index = tile_index;
            pixel_schema->obj_priority = tile_attr & BIT_7_MASK;
            pixel_schema->      y_flip = tile_attr & BIT_6_MASK;
            pixel_schema->      x_flip = tile_attr & BIT_5_MASK;
            pixel_schema-> dmg_palette = (tile_attr & BIT_4_MASK) >> 4;
            pixel_schema->        bank = (tile_attr & BIT_3_MASK) >> 3;
            pixel_schema-> gbc_palette = tile_attr & LOWER_3_MASK; 
            enqueue(obj_scan, pixel_schema);
        }
        current_entry += OAM_ENTRY_SIZE;
   }
   if (!is_empty(obj_scan)) sort_oam_by_xpos(obj_scan);
}

static bool drawing_window(uint8_t lx, uint8_t ly)
{
    bool win_enabled = (bool)(memory[LCDC] & BIT_5_MASK);
    uint8_t WX = memory[WX];
    uint8_t WY = memory[WY];
    return ((lx >= WX) && (ly >= WY) && win_enabled); 
}

static uint32_t get_argb(uint8_t lsb, uint8_t msb)
{
    uint16_t color = (msb << BYTE) | lsb;
    uint8_t   red  = ((color)       & LOWER_5_MASK) << 3;
    uint8_t green  = ((color >>  5) & LOWER_5_MASK) << 3;
    uint8_t  blue  = ((color >> 10) & LOWER_5_MASK) << 3;
    uint32_t argb  = 
    (0xFF << (BYTE * 3)) | (red << (BYTE * 2)) | (blue << (BYTE * 1)) | green;
    return argb;
}

static uint8_t copy_bgw_attributes(uint16_t tma)
{ // always pull from bank 1
    uint8_t attributes = *read_vram(TILE_MAP_BANK_1, tma);
    pixel_schema->bg_priority = (bool)    (attributes & BIT_7_MASK);
    pixel_schema->y_flip      = (bool)    (attributes & BIT_6_MASK);
    pixel_schema->x_flip      = (bool)    (attributes & BIT_5_MASK);
    pixel_schema->bank        = (uint8_t) ((attributes >> 3) & BIT_0_MASK);
    pixel_schema->gbc_palette = (uint8_t) (attributes & LOWER_3_MASK);
    return pixel_schema->bank;
}

static void empty_pixel(uint8_t lx, uint8_t ly)
{
    pixel_schema->color_id = IS_ZERO;
    pixel_schema->  x_draw = lx;
    pixel_schema->  y_draw = ly;
}

static void load_bg_pixel(uint8_t lx, uint8_t ly)
{
    // Time Map Index (tmi) Calculation.
    uint8_t    scx  = memory[SCX];
    uint8_t    scy  = memory[SCY];
    uint8_t tile_x  = ((scx + lx) / TILE_SIZE) % GRID_SIZE;
    uint8_t  pix_x  = (scx + lx) % TILE_SIZE;
    uint8_t tile_y  = ((scy + ly) / TILE_SIZE) % GRID_SIZE;
    uint8_t  pix_y  = (scy + ly) % TILE_SIZE;
    bool map_select = ((bool) (memory[LCDC] & BIT_3_MASK)); // Tile Map $9C00?  
    uint16_t    tmb = map_select ? TM1_ADDRESS_START : TM0_ADDRESS_START;
    uint16_t    tma = tmb + tile_x + (tile_y * GRID_SIZE);
    uint8_t    bank = is_gbc() ? copy_bgw_attributes(tma) : TILE_MAP_BANK_0;
    if (pixel_schema->y_flip) pix_y = TILE_SIZE - 1 - pix_y;
    if (pixel_schema->x_flip) pix_x = TILE_SIZE - 1 - pix_x;
    // Obtaining Tile Data.
    uint8_t line_lsb, line_msb;
    bool method = (bool) (memory[LCDC] & BIT_4_MASK);
    uint16_t tile_index = method ? // True = $8000U, False = $9000S
    (B0_ADDRESS_START + ((uint8_t) *read_vram(bank, tma) * TILE_SIZE * 2) + (pix_y * 2)):
    (B2_ADDRESS_START + ( (int8_t) *read_vram(bank, tma) * TILE_SIZE * 2) + (pix_y * 2));
    line_lsb = *read_vram(bank, tile_index);
    line_msb = *read_vram(bank, tile_index + 1);
    // Load pixel into queue.
    bool color_lsb = (line_lsb >> (TILE_SIZE - 1 - pix_x)) & BIT_0_MASK;
    bool color_msb = (line_msb >> (TILE_SIZE - 1 - pix_x)) & BIT_0_MASK;
    pixel_schema->color_id = (color_msb << 1) | color_lsb;
    pixel_schema->x_draw = lx;
    pixel_schema->y_draw = ly;
    enqueue(bgw_fifo, pixel_schema);
}

static void load_win_pixel(uint8_t lx, uint8_t ly)
{
    // Time Map Index (tmi) Calculation. (Relative positioning for Window) (NO WRAPPING)
    uint8_t       wx = memory[WX];
    uint8_t       wy = memory[WY];
    uint8_t   tile_x = (lx - wx) / TILE_SIZE;
    uint8_t    pix_x = (lx - wx) % TILE_SIZE;
    uint8_t   tile_y = (ly - wy) / TILE_SIZE;
    uint8_t    pix_y = (ly - wy) % TILE_SIZE;
    bool map_select = ((bool) (memory[LCDC] & BIT_5_MASK)); // Tile Map $9C00?  
    uint16_t    tmb = map_select ? TM1_ADDRESS_START : TM0_ADDRESS_START;
    uint16_t    tma = tmb + tile_x + (tile_y * GRID_SIZE); 
    uint8_t     bank = is_gbc() ? copy_bgw_attributes(tma) : TILE_MAP_BANK_0;
    if (pixel_schema->y_flip) pix_y = TILE_SIZE - 1 - pix_y;
    if (pixel_schema->x_flip) pix_x = TILE_SIZE - 1 - pix_x;
    // Obtaining Tile Data.
    uint8_t line_lsb, line_msb;
    bool method = (bool) (memory[LCDC] & BIT_4_MASK);
    uint16_t tli = method ? // True = $8000U, False = $9000S
    (B0_ADDRESS_START + ((uint8_t) *read_vram(bank, tma) * TILE_SIZE * 2) + (pix_y * 2)):
    (B2_ADDRESS_START + ( (int8_t) *read_vram(bank, tma) * TILE_SIZE * 2) + (pix_y * 2));
    line_lsb = *read_vram(bank, tli);
    line_msb = *read_vram(bank, tli + 1);
    // Load pixel into queue.
    bool color_lsb = (line_lsb >> (TILE_SIZE - 1 - pix_x)) & BIT_0_MASK;
    bool color_msb = (line_msb >> (TILE_SIZE - 1 - pix_x)) & BIT_0_MASK;
    pixel_schema->color_id = (color_msb << 1) | color_lsb;
    pixel_schema->x_draw = lx;
    pixel_schema->y_draw = ly;
    enqueue(bgw_fifo, pixel_schema);
}

static void load_bgw(uint8_t ly)
{
    uint8_t lx = 0;
    while (lx < GBC_WIDTH)
    {
        drawing_window(lx, ly)? 
        load_win_pixel(lx, ly): 
        load_bg_pixel(lx, ly);
        lx += 1; 
    }
}

static void load_obj(uint8_t ly)
{ // Assume OAM Scan has been completed.
    uint8_t        lx = 0;
    GbcPixel *current = dequeue(obj_scan);
    bool      stacked = (bool) (memory[LCDC] & BIT_2_MASK); 
    while(lx < GBC_WIDTH)
    {
        if (current == NULL)
        {
            empty_pixel(lx, ly);
            enqueue(obj_fifo, pixel_schema);
        }
        else if (lx < current->x_obj)
        {
            empty_pixel(lx, ly);
            enqueue(obj_fifo, pixel_schema);
        }
        else if 
        (
            lx >= current->x_obj && 
            lx < (current->x_obj + TILE_SIZE + (TILE_SIZE * stacked))
        )
        {
            // Logic to enqueue object pixel based on lx, ly, x-flip, y-flip, and obj_size.
            uint8_t pix_x = lx - current->x_obj;
            uint8_t pix_y = ly - current->y_obj;
            if (current->y_flip) pix_y = ((stacked * TILE_SIZE) + (TILE_SIZE - 1)) - pix_y;
            if (current->x_flip) pix_x = (TILE_SIZE - 1) - pix_x;
            // Use $8000U addressing
            uint8_t       tli = B0_ADDRESS_START + (current->tile_index * TILE_SIZE * 2) + (2 * pix_y);
            uint8_t  line_lsb = *read_vram(current->bank, tli);
            uint8_t  line_msb = *read_vram(current->bank, tli + 1);
            bool    color_lsb = (bool) (line_lsb | (BIT_7_MASK >> pix_x));
            bool    color_msb = (bool) (line_msb | (BIT_7_MASK >> pix_x));
            uint8_t     color = (color_msb << 1) | (color_lsb);
            pixel_schema->color_id = color;
            pixel_schema->  x_draw = lx;
            pixel_schema->  y_draw = ly;   
            enqueue(obj_fifo, pixel_schema);
        }   
        else
        {
            current = dequeue(obj_scan);
            continue;
        }
        lx += 1;
    }
}

static void scanline(uint8_t ly)
{
    load_bgw(ly);
    load_obj(ly);
}

static void draw_gbc(const GbcPixel *pixel, bool is_obj)
{
    const uint8_t *color_palette = read_cram(is_obj, pixel->gbc_palette, pixel->color_id);
    uint8_t            lsb = color_palette[0];
    uint8_t            msb = color_palette[1];
    uint32_t        color  = get_argb(lsb, msb);
    lcd[pixel->x_draw + (GBC_WIDTH * pixel->y_draw)] = color;  
}

static void merge_fifos_gbc()
{
    while (!is_empty(bgw_fifo))
    {
        GbcPixel *bgw = dequeue(bgw_fifo);
        GbcPixel *obj = dequeue(obj_fifo);

        if (!(memory[LCDC] & BIT_0_MASK) || obj->color_id == 0)
        {
            draw_gbc(bgw, false);
        }
        else if (!bgw->color_id)
        {  
            draw_gbc(obj, true);
        }
        else if (!obj->obj_priority)
        {
            draw_gbc(obj, true);
        }
        else
        {
            draw_gbc(bgw, false);
        }
    }  
}

static void draw_bgw_dmg(GbcPixel *pixel)
{
    uint8_t   bgp = memory[BGP];
    uint8_t color = (bgp >> (2 * pixel->color_id)) & LOWER_2_MASK;
    uint8_t     x = pixel->x_draw;
    uint8_t     y = pixel->y_draw;
    switch (color)
    {
        case 0: 
            lcd[x + (GBC_WIDTH * y)] = WHITE;
            break; 
        case 1: 
            lcd[x + (GBC_WIDTH * y)] = LIGHT_GRAY;
            break;
        case 2: 
            lcd[x + (GBC_WIDTH * y)] = DARK_GRAY;
            break;
        case 3: 
            lcd[x + (GBC_WIDTH * y)] = BLACK;
            break;
        default:
            lcd[x + (GBC_WIDTH * y)] = BLACK;
            break;
    }
}

static void draw_obj_dmg(GbcPixel *pixel)
{ // Differentiate (OBP0 - OBP1) via dmg_palette attribute.
    uint8_t obp =
    pixel->dmg_palette ? 
    (memory[OBP0]): 
    (memory[OBP1]);
    uint8_t color = obp >> (2 * pixel->color_id);
    uint8_t     x = pixel->x_draw;
    uint8_t     y = pixel->y_draw;
    switch (color)
    {
        case 0: 
            lcd[x + (GBC_WIDTH * y)] = WHITE;
            break; 
        case 1: 
            lcd[x + (GBC_WIDTH * y)] = LIGHT_GRAY;
            break;
        case 2: 
            lcd[x + (GBC_WIDTH * y)] = DARK_GRAY;
            break;
        case 3: 
            lcd[x + (GBC_WIDTH * y)] = BLACK;
            break;
    }
}

static void merge_fifos_dmg()
{
    while (!is_empty(bgw_fifo))
    {
        GbcPixel *bgw = dequeue(bgw_fifo);
        GbcPixel *obj = dequeue(obj_fifo);
        if (!obj->color_id)
        {
            draw_bgw_dmg(bgw);
        }
        else if (obj->obj_priority && bgw->color_id != 0)
        {
            draw_bgw_dmg(bgw);
        }
        else
        {
            draw_obj_dmg(obj);
        }
    }
}

static void render_scanline(uint8_t ly)
{
    oam_scan(ly);
    scanline(ly);
    is_gbc() ? merge_fifos_gbc() : merge_fifos_dmg();
}

void *render_frame()
{   
    uint8_t ly = 0;
    while (ly < GBC_HEIGHT && (memory[LCDC] & BIT_7_MASK))
    {
        render_scanline(ly);
        ly += 1;
    }
    return lcd;
}

// Emulation interface for graphics.
void dot(uint32_t current_dot)
{
    uint16_t dot_sc = current_dot % DOTS_PER_LINE;
    uint8_t      ly = (uint8_t) (current_dot / DOTS_PER_LINE);
    uint8_t     lyc = (uint8_t) memory[LYC];
    uint8_t    stat = (uint8_t) memory[STAT];
    if (ly < GBC_HEIGHT && dot_sc == 0)
    { // OAM Start
        stat = (stat & ~LOWER_2_MASK) | OAM_SCAN;
        if (stat & BIT_5_MASK) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    }
    else if (ly < GBC_HEIGHT && dot_sc == 80)
    { // Drawing Start
        stat = (stat & ~LOWER_2_MASK) | DRAWING;
    }
    else if (dot_sc == 369)
    { // HBlank Start
        stat = (stat & ~LOWER_2_MASK) | HBLANK;
        if (stat & BIT_3_MASK) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    }
    else if (ly == GBC_HEIGHT && dot_sc == 0)
    { // VBlank Start
        request_interrupt(VBLANK_INTERRUPT_CODE);
        stat = (stat & ~LOWER_2_MASK) | VBLANK;
        if (stat & BIT_4_MASK) request_interrupt(LCD_STAT_INTERRUPT_CODE); 
    }
    bool lyc_triggered = (ly == lyc);
    stat = ((uint8_t) (stat & ~BIT_2_MASK)) | ((uint8_t) (lyc_triggered << 2));
    if (lyc_triggered && (stat & BIT_6_MASK)) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    memory[LY]   = ly;
    memory[STAT] = stat;
}