#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "cart.h"   // Used to determine DMG or CGB
#include "common.h" // Essential enums for readibility
#include "cpu.h"    // Interrupt requesting
#include "logger.h" // Console or file logs
#include "mmu.h"    // Reading hardware memory
#include "ppu.h"    // Header file
#include "util.h"   // Queue implementation

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

typedef struct
{
    uint8_t     x;
    uint8_t     y;
    uint8_t index;
    uint8_t  attr;
    uint8_t   lsb;
    uint8_t   msb;

} Tile;

typedef struct
{
    // 160px by 144px LCD Display
    uint32_t   *lcd;
    // Control
    uint8_t   *lcdc;
    uint8_t   *stat;
    uint8_t    *lyc;
    // Scanline
    uint8_t penalty;
    uint8_t      lx;
    uint8_t     *ly;
    // Background
    uint8_t    *scx;
    uint8_t    *scy;
    // Window
    uint8_t     *wx;
    uint8_t     *wy;
    // DMG Palettes
    uint8_t    *bgp;
    uint8_t   *opd0;
    uint8_t   *opd1;
    // Flags
    bool   sc_complete;

} PpuState;

static PpuState *ppu;
static Tile    *tile;

static GbcPixel *pixel_schema;
static Queue        *scanline;
static Queue        *oam_fifo;

/* ================== GLOBAL        ================== */

static void reset_ppu(PpuState *ppu)
{
    ppu->           lx =     0;
    ppu->      penalty =     0;
    ppu->  sc_complete = false;
}

static void init_registers(PpuState *reg)
{
    reg->lcdc = get_memory_pointer(LCDC);
    reg->stat = get_memory_pointer(STAT);
    reg->  ly = get_memory_pointer(  LY);
    reg-> lyc = get_memory_pointer( LYC); 
    
    reg-> scx = get_memory_pointer( SCX);
    reg-> scy = get_memory_pointer( SCY);
    
    reg->  wx = get_memory_pointer(  WX);
    reg->  wy = get_memory_pointer(  WY);

    reg-> bgp = get_memory_pointer( BGP);
    reg->opd0 = get_memory_pointer(OBP0);
    reg->opd1 = get_memory_pointer(OBP1); 
}

static void set_ppu_mode(PpuState *ppu, PpuMode mode)
{
    (*ppu->stat) = ((*ppu->stat) & ~LOWER_2_MASK) | mode;
}

/* ================== DRAWING       ================== */

static GbcPixel *empty_pixel()
{
    pixel_schema->color_id     = 0;
    pixel_schema->bg_priority  = false;
    pixel_schema->obj_priority = false;
    return pixel_schema;
}

static uint32_t get_argb(uint8_t lsb, uint8_t msb)
{
    uint16_t color = (msb << BYTE) | lsb;
    uint8_t   red  = ((color)       & LOWER_5_MASK) << 3;
    uint8_t green  = ((color >>  5) & LOWER_5_MASK) << 3;
    uint8_t  blue  = ((color >> 10) & LOWER_5_MASK) << 3;
    uint32_t argb  = 
    (0xFF << (BYTE * 3)) | (red << (BYTE * 2)) | (green << (BYTE * 1)) | blue;
    return argb;
}

static uint32_t get_dmg_shade(uint8_t id)
{
    uint32_t result = WHITE;
    switch(id)
    {
        case 0: result =      WHITE; break;
        case 1: result = LIGHT_GRAY; break;
        case 2: result =  DARK_GRAY; break;
        case 3: result =      BLACK; break;
    }
    return result;
}

static uint8_t get_next_color(Tile *tile, bool x_flip)
{
    uint8_t lsb, msb;

    if (!x_flip)
    {
        lsb = (tile->lsb & BIT_7_MASK) >> 7;
        tile->lsb <<= 1;
        msb = (tile->msb & BIT_7_MASK) >> 7;
        tile->msb <<= 1;
    }
    else
    {
        lsb = tile->lsb & BIT_0_MASK;
        tile->lsb >>= 1;
        msb = tile->msb & BIT_0_MASK;
        tile->msb >>= 1;
    }

    return ((msb << 1) | lsb);
}

static bool drawing_window(PpuState *ppu)
{
    uint8_t lcdc = (*ppu->lcdc);
    uint8_t   lx =  (ppu->lx); uint8_t ly = (*ppu->ly);
    uint8_t   wx = (*ppu->wx); uint8_t wy = (*ppu->wy);
    return ((lx >= (wx - 7)) && (ly >= wy) && (lcdc & BIT_5_MASK));
}

static bool drawing_obj(GbcPixel *bgw, GbcPixel *obj, PpuState *ppu)
{
    bool obj_enabled = (((*ppu->lcdc) & BIT_1_MASK) != 0);
    bool master_prio = is_gbc() ? ((*ppu->lcdc & BIT_0_MASK) != 0) : false;

    if (!obj_enabled)       return false;

    if (bgw->color_id == 0) return true;

    if (!master_prio)       return true;

    if (!bgw->bg_priority && !obj->obj_priority) return true;

    return false;
}

static uint32_t get_obj_pixel_color(GbcPixel *pixel)
{
    if(is_gbc())
    {
        uint8_t lsb = read_cram(true, pixel->gbc_palette, pixel->color_id, 0);
        uint8_t msb = read_cram(true, pixel->gbc_palette, pixel->color_id, 1);
        return get_argb(lsb, msb); 
    }
    else
    {
        uint8_t opd = (pixel->dmg_palette) ? io_memory_read(OBP1) : io_memory_read(OBP0);
        uint8_t cid = (opd >> (2 * pixel->color_id)) & LOWER_2_MASK;
        return get_dmg_shade(cid);
    }
}

static uint32_t get_bgw_pixel_color(GbcPixel *pixel)
{
    if(is_gbc())
    {
        uint8_t lsb = read_cram(false, pixel->gbc_palette, pixel->color_id, 0);
        uint8_t msb = read_cram(false, pixel->gbc_palette, pixel->color_id, 1);
        return get_argb(lsb, msb); 
    }
    else
    {
        uint8_t cid = ((*ppu->bgp) >> (2 * pixel->color_id)) & LOWER_2_MASK;
        return get_dmg_shade(cid);
    }
}

/* ================== VRAM ACCESS ================== */

static uint16_t bgw_tile_data_address(Tile *tile, uint8_t lcdc, uint8_t row)
{
    uint16_t address;
    if ((lcdc & BIT_4_MASK) != 0) // $8000 Unsigned Method
    {
        uint8_t tile_index = (uint8_t) tile->index;
        address = B0_ADDRESS_START + (16 * tile_index) + (2 * row);
    }
    else                          // $9000 Signed Method
    {
        int8_t tile_index = (int8_t) tile->index;
        address = B2_ADDRESS_START + (16 * tile_index) + (2 * row);
    }
    return address;
}

static void get_win_tile(Tile *tile, PpuState *ppu) // Encodes (index, attr, lsb, msb)
{
    uint8_t      lcdc = (*ppu->lcdc);
    uint8_t        lx =      (ppu->lx); uint8_t ly = (*ppu->ly);
    uint8_t        wx =    (*ppu->scx); uint8_t wy = (*ppu->wy);

    uint8_t    tile_x = (lx - (wx - 7)) / TILE_SIZE;
    uint8_t    tile_y = (ly - wy) / TILE_SIZE;
    uint16_t     base = ((lcdc & BIT_6_MASK) != 0) ? TM1_ADDRESS_START : TM0_ADDRESS_START;
    uint16_t  address = base + (tile_y * GRID_SIZE) + tile_x;
    
    uint8_t       row = (ly - wy) % TILE_SIZE;
    uint8_t      bank = TILE_MAP_BANK_0;
    tile->      index = read_vram(TILE_MAP_BANK_0, address);

    if (is_gbc())
    {
        tile->attr = read_vram(TILE_MAP_BANK_1, address);
        bank = (tile->attr & BIT_3_MASK) >> 3;
        row  = (tile->attr & BIT_6_MASK) ? (TILE_SIZE - 1 - row) : row; 
    }

    address = bgw_tile_data_address(tile, lcdc, row);
    tile->       lsb = read_vram(bank, address);
    tile->       msb = read_vram(bank, address + 1);
}

static void get_bg_tile(Tile *tile, PpuState *ppu)  // Encodes (index, attr, lsb, msb)
{
    uint8_t      lcdc = (*ppu->lcdc);

    uint8_t        lx =   (ppu->lx); uint8_t  ly =  (*ppu->ly);
    uint8_t       scx = (*ppu->scx); uint8_t scy = (*ppu->scy);
    
    uint8_t    tile_x = ((scx + lx) / TILE_SIZE) & LOWER_5_MASK;
    uint8_t    tile_y = ((ly + scy) / TILE_SIZE) % GRID_SIZE;
    uint16_t     base = ((lcdc & BIT_3_MASK) != 0) ? TM1_ADDRESS_START : TM0_ADDRESS_START;
    uint16_t  address = base + (tile_y * GRID_SIZE) + tile_x;
    
    uint8_t       row = (ly + scy) % TILE_SIZE;
    uint8_t      bank = TILE_MAP_BANK_0;
    tile->      index = read_vram(TILE_MAP_BANK_0, address);

    if (is_gbc())
    {
        tile->attr = read_vram(TILE_MAP_BANK_1, address);
        bank = (tile->attr & BIT_3_MASK) >> 3;
        row  = ((tile->attr & BIT_6_MASK) != 0) ? (TILE_SIZE - 1 - row) : row; 
    }

    address = bgw_tile_data_address(tile, lcdc, row);
    tile->       lsb = read_vram(bank, address);
    tile->       msb = read_vram(bank, address + 1);
}

static void get_obj_tile(Tile *tile, GbcPixel *obj, PpuState *ppu)
{
    uint8_t   bank = obj->bank;
    uint8_t    row = (*ppu->ly - obj->y);
    bool   stacked = ((*ppu->lcdc & BIT_2_MASK) != 0);
    uint8_t height = stacked ? 16 : 8;
    row = (obj->y_flip) ? (row - height - 1) : row;

    tile->index = obj->tile_index; 

    uint16_t address = B0_ADDRESS_START + (tile->index * 16) + (row * 2);
    tile->lsb = read_vram(bank, address);
    tile->msb = read_vram(bank, address + 1);
}

static void oam_scan(uint8_t ly)
{
    reset_queue(oam_fifo);
    uint16_t curr_address = OAM_ADDRESS_START;
    uint8_t lcdc = (*ppu->lcdc);
    bool stacked = (lcdc & BIT_2_MASK) != 0;

    while(curr_address <= OAM_ADDRESS_END)
    {
        uint8_t      y_pos = read_memory(curr_address) - 16;
        uint8_t     height = (stacked) ? 16 : 8;
        bool   on_scanline = (ly >= y_pos) && ((ly - y_pos) < height);

        if (on_scanline)
        {
            GbcPixel     *object = empty_pixel();
            uint8_t        x_pos = read_memory(curr_address + 1);
            uint8_t   tile_index = read_memory(curr_address + 2);
            uint8_t   attributes = read_memory(curr_address + 3);
            object-> oam_address = curr_address;
            object->           x = x_pos - 8;
            object->           y = y_pos;
            object->  tile_index = tile_index;
            object->      is_obj = true;
            object->obj_priority = (attributes & BIT_7_MASK) != 0;
            object->      y_flip = (attributes & BIT_6_MASK) != 0;
            object->      x_flip = (attributes & BIT_5_MASK) != 0;
            object-> dmg_palette = (attributes & BIT_4_MASK) != 0;
            object->        bank = (attributes & BIT_3_MASK) != 0;
            object-> gbc_palette = (uint8_t) (attributes & LOWER_3_MASK);
            cpu_log(DEBUG, "Found object at (%02X, %02X)", object->x, object->y);
            enqueue(oam_fifo, object);
        }

        curr_address += OAM_ENTRY_SIZE;
    }

    sort_oam_by_xpos(oam_fifo);
}

/* ================== SCANLINE RENDER ============= */

static void render_background(PpuState *ppu, Queue *scanline)
{
    ppu->lx = 0;
    bool win_rendering = false;

    while (ppu->lx < GBC_WIDTH)
    {
        win_rendering ? get_win_tile(tile, ppu) : get_bg_tile(tile, ppu);

        uint8_t offset = ppu->lx % 8;
        // Non-zero offset means we transitioned to window tile at an uneven spacing.
        while (offset != 0) 
        {
            get_next_color(tile, false);
            offset -= 0;
        }

        for (int i = 0; i < TILE_SIZE; i++)
        {
            GbcPixel    *pixel = empty_pixel();
            bool x_flip = is_gbc() ? ((tile->attr & BIT_5_MASK) != 0) : false;
            pixel->   color_id = get_next_color(tile, x_flip);
            pixel->bg_priority = is_gbc() ? ((tile->attr & BIT_7_MASK) != 0) : 0;
            pixel->gbc_palette = is_gbc() ? (tile->attr & LOWER_3_MASK) : 0;
            pixel->      is_obj = false;
            enqueue(scanline, pixel);

            ppu->lx += 1;

            if (!win_rendering && drawing_window(ppu))
            {
                win_rendering = true; // for rest of scanline
                break;
            }
        }
    }
}

static void render_objects(PpuState *ppu, Queue *scanline, Queue *oam_fifo)
{
    if (is_empty(oam_fifo)) return;

    ppu->lx = 0;

    GbcPixel *obj = dequeue(oam_fifo);

    while ((obj != NULL) && (ppu->lx < GBC_WIDTH))
    {
        if (ppu->lx >= obj->x)
        {
            get_obj_tile(tile, obj, ppu);
            uint8_t offset = ppu->lx - obj->x;

            for (int i = 0; i < TILE_SIZE; i++)
            {
                obj->color_id = get_next_color(tile, obj->x_flip);

                if (i < offset) continue;

                GbcPixel *bgw = dequeue(scanline);
                drawing_obj(bgw, obj, ppu) ? enqueue(scanline, obj) : enqueue(scanline, bgw);
                ppu->lx += 1;
            }
            
            obj = dequeue(oam_fifo);
        }
        else // Circular shift scanline.
        {
            enqueue(scanline, dequeue(scanline));
            ppu->lx += 1;
        }
    }
    
    while (ppu->lx < GBC_WIDTH) // Complete scanline shift.
    {
        enqueue(scanline, dequeue(scanline));
        ppu->lx += 1;
    }
}   

static void render_scanline(PpuState *ppu)
{
    render_background(ppu, scanline);
    render_objects(ppu, scanline, oam_fifo);
    ppu->lx = 0; uint8_t ly = (*ppu->ly);
    while (!is_empty(scanline))
    {   
        GbcPixel *pixel = dequeue(scanline);
        ppu->lcd[ly * GBC_WIDTH + ppu->lx] = 
        pixel->is_obj ? get_obj_pixel_color(pixel) : get_bgw_pixel_color(pixel);
        ppu->lx += 1; 
    }
}

static void prep_scanline_render(PpuState *ppu)
{   
    reset_ppu(ppu);
    reset_queue(scanline);
}

/* ================== PUBLIC API ================= */

void dot(uint32_t current_dot)
{
    uint16_t sc_dot = current_dot % DOTS_PER_LINE;
    uint8_t      ly = (current_dot / DOTS_PER_LINE);
    uint8_t    lcdc = (*ppu->lcdc);
    uint8_t    stat = (*ppu->stat);

    (*ppu->ly) = ly; // Recording scanline position.

    if      ((ly < GBC_HEIGHT)  && (sc_dot ==   0))
    {
        set_ppu_mode(ppu, OAM_SCAN);
        oam_scan(ly);
        bool triggered = ((stat & BIT_5_MASK) != 0);
        if (triggered) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    }
    else if ((ly < GBC_HEIGHT)  && (sc_dot ==  80))
    {
        set_ppu_mode(ppu, DRAWING);
        prep_scanline_render(ppu);
    }
    else if ((ly < GBC_HEIGHT)  && (sc_dot == 369))
    {
        render_scanline(ppu);
        set_ppu_mode(ppu, HBLANK);
        bool triggered = ((stat & BIT_4_MASK) != 0);
        if (triggered) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    }
    else if ((ly == GBC_HEIGHT) && (sc_dot ==   0))
    {
        set_ppu_mode(ppu, VBLANK);
        request_interrupt(VBLANK_INTERRUPT_CODE); // Happens once per frame, regardless.
        bool triggered = ((stat & BIT_4_MASK) != 0);
        if (triggered) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    }

    uint8_t    lyc = (*ppu->lyc);
    bool triggered = ((ly == lyc) && (stat & BIT_6_MASK));
    if (triggered) request_interrupt(LCD_STAT_INTERRUPT_CODE);
}

void *render_frame()
{   
    return ppu->lcd;
}

bool init_graphics()
{
    ppu           = (PpuState*) malloc(sizeof(PpuState));
    ppu->lcd      = (uint32_t*) malloc(GBC_WIDTH * GBC_HEIGHT * sizeof(uint32_t));
    reset_ppu(ppu); init_registers(ppu);

    tile          = (Tile*) malloc(sizeof(Tile)); 

    pixel_schema  = (GbcPixel*) malloc(sizeof(GbcPixel));
    scanline      = init_queue(GBC_WIDTH);
    oam_fifo      = init_queue(10);
}

void tidy_graphics()
{
    free(ppu->lcd);         ppu->lcd = NULL;
    free(ppu);                   ppu = NULL;
    free(tile);                 tile = NULL;
    free(pixel_schema); pixel_schema = NULL;
    tidy_queue(scanline);
    tidy_queue(oam_fifo);
}

