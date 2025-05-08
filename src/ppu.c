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

typedef enum
{
    GET_TILE     = 1,
    GET_TDL      = 2,
    GET_TDH      = 3,
    SLEEP        = 4,
    PUSH         = 5,
    PAD_OAM_FIFO = 6,

} FetcherStep;

typedef struct
{
    FetcherStep   step;

    uint8_t   duration;
    uint8_t       tile;
    uint8_t        lsb;
    uint8_t        msb;
    uint8_t       attr;
    uint8_t          x;

    bool    row_pushed;

    (*handler)(struct PpuState*);

} PixelFetcher;

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
    bool obj_rendering;
    bool win_rendering;

} PpuState;

static PpuState              *ppu;

static PixelFetcher  *bgw_fetcher;
static PixelFetcher  *obj_fetcher;
static GbcPixel     *pixel_schema;
static Queue        *bgw_pix_fifo;
static Queue        *obj_pix_fifo;
static Queue            *oam_fifo;

/* ================== GLOBAL        ================== */

static void reset_fetcher(PixelFetcher *fetcher)
{
    fetcher->      step = GET_TILE;
    fetcher->  duration =        0;
    fetcher->       lsb =        0;
    fetcher->       msb =        0;
    fetcher->      tile =        0;
    fetcher->      attr =        0;
    fetcher->         x =        0;
    fetcher->row_pushed =    false;
}

static void reset_ppu(PpuState *ppu)
{
    ppu->           lx =     0;
    ppu->           ly =     0;
    ppu->      penalty =     0;

    ppu->  sc_complete = false;
    ppu->obj_rendering = false;
    ppu->win_rendering = false;
}

static void init_registers(PpuState *reg)
{
    reg->lcdc = get_memory_pointer(LCDC);
    reg->stat = get_memory_pointer(STAT);
    reg-> lyc =  get_memory_pointer(LYC); 
    
    reg-> scx =  get_memory_pointer(SCX);
    reg-> scy =  get_memory_pointer(SCY);
    
    reg->  wx =   get_memory_pointer(WX);
    reg->  wy =   get_memory_pointer(WY);

    reg-> bgp =  get_memory_pointer(BGP);
    reg->opd0 = get_memory_pointer(OBP0);
    reg->opd1 = get_memory_pointer(OBP1); 
}

/* ================== DRAWING       ================== */

/* DRAW METHODOLOGY | -------> COLOR AND EMPTY PIXEL   */

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

/* DRAW METHODOLOGY | -------> LOGIC AND CONTROL FLOW */

static bool drawing_window(PpuState *ppu)
{
    uint8_t lcdc = (*ppu->lcdc);
    uint8_t   lx =  (ppu->lx); uint8_t ly = (*ppu->ly);
    uint8_t   wx = (*ppu->wx); uint8_t wy = (*ppu->wy);
    return ((lx >= (wx - 7)) && (ly >= wy) && (lcdc & BIT_5_MASK));
}

static bool obj_on_scanline(PpuState *ppu)
{
    if (is_empty(oam_fifo)) return false;
    GbcPixel *obj = peek(oam_fifo);
    return ((ppu->lx) == (obj->x - 8)) && ((*ppu->lcdc) & BIT_1_MASK);
}

static bool drawing_obj(GbcPixel *obj, GbcPixel *bgw, PpuState *ppu)
{ // Merge logic
    
    if (is_gbc()) // CGB
    {
        bool bgw_disabled = (((*ppu->lcdc) & BIT_0_MASK) == 0); 

        if (bgw_disabled || (obj->color_id == 0)) 
        {
            return false;
        }

        if (bgw-> color_id == 0)
        {
            return true;
        }

        if (obj->obj_priority == 0)
        {
            return true;
        }

        return false;
    }
    
    // DMG
    
    if (obj->color_id != 0)
    {
        return false;
    }

    if ((obj->obj_priority != 0) && (bgw->color_id != 0))
    {
        return false;
    }

    return true;   
    
}

/* DRAW METHODOLOGY | -------> DRAWING PIXELS        */

static uint32_t draw_obj_pixel(GbcPixel *pixel)
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

static uint32_t draw_bgw_pixel(GbcPixel *pixel)
{
    if(is_gbc())
    {
        uint8_t lsb = read_cram(false, pixel->gbc_palette, pixel->color_id, 0);
        uint8_t msb = read_cram(false, pixel->gbc_palette, pixel->color_id, 1);
        return get_argb(lsb, msb); 
    }
    else
    {
        uint8_t bpd = io_memory_read(BGP);
        uint8_t cid = (bpd >> (2 * pixel->color_id)) & LOWER_2_MASK;
        return get_dmg_shade(cid);
    }
}

static void draw_pixel_lcd(PpuState *ppu)
{  
    if (!(lcdc & BIT_7_MASK))
    { // Black screen!
        dequeue(obj_pix_fifo);
        dequeue(bgw_pix_fifo);
        lcd[lx + (GBC_WIDTH * ly)] = BLACK;
        advance_renderer();
        return;
    }

    if (!win_rendering && drawing_window(lx, ly, lcdc))
    { // Start rendering window.
        win_rendering = true;
        reset_bgw_fetcher();
        reset_queue(bgw_pix_fifo);
        return;
    }

    uint32_t color;
    if (!is_empty(obj_pix_fifo) && !is_empty(bgw_pix_fifo))
    {
        GbcPixel  *obj = dequeue(obj_pix_fifo);
        GbcPixel  *bgw = dequeue(bgw_pix_fifo);
        color          = drawing_obj(obj, bgw, lcdc) ? draw_obj_pixel(obj) : draw_bgw_pixel(bgw);
        lcd[lx + (GBC_WIDTH * ly)] = color;
        advance_renderer();
        return;
    }
    
    if(!is_empty(bgw_pix_fifo))
    {
        GbcPixel  *bgw = dequeue(bgw_pix_fifo);
        color          = draw_bgw_pixel(bgw);
        lcd[lx + (GBC_WIDTH * ly)] = color;
        advance_renderer();
        return;
    }
}

/* ================== VRAM ACCESS ================== */

/* VRAM ACCESS      | -------> OBTAINING TILE DATA   */

static uint16_t bgw_tile_data_address(PpuState *ppu, uint8_t row)
{
    uint16_t address;
    if (((*ppu->lcdc) & BIT_4_MASK) != 0)
    { // $8000 Unsigned Method
        uint8_t tile_index = (uint8_t) bgw_fetcher->tile;
        address = B0_ADDRESS_START + (16 * tile_index) + (2 * row);

    }
    else
    { // $9000 Signed Method
        int8_t tile_index = (int8_t) bgw_fetcher->tile;
        address = B2_ADDRESS_START + (16 * tile_index) + (2 * row);
    }
    return address;
}

static void get_win_tile(PpuState *ppu, PixelFetcher *bgw)
{
    uint8_t    tile_x = (ppu->lx - ((*ppu->wx) - 7)) / TILE_SIZE;
    uint8_t    tile_y = ((*ppu->ly) - (*ppu->wy)) / TILE_SIZE;
    uint16_t     base = ((*ppu->lcdc) & BIT_6_MASK) ? TM1_ADDRESS_START : TM0_ADDRESS_START;
    uint16_t  address = base + (tile_y * GRID_SIZE) + tile_x;
    bgw_fetcher->tile = read_vram(TILE_MAP_BANK_0, address);
    if (is_gbc()) bgw_fetcher->attr = read_vram(TILE_MAP_BANK_1, address);
}

static void get_win_tile_data(PpuState *ppu)
{
    uint8_t       wy = io_memory_read(WY);
    uint8_t      row = (ly - wy) % TILE_SIZE;
    uint8_t     bank = TILE_MAP_BANK_0;

    if (is_gbc())
    {
        bank = (bgw_fetcher->attr & BIT_3_MASK) >> 3;
        row  = (bgw_fetcher->attr & BIT_6_MASK) ? (TILE_SIZE - 1 - row) : row; 
    }

    uint16_t address = bgw_tile_data_address(lcdc, row);
    bgw_fetcher->lsb = read_vram(bank, address);
    bgw_fetcher->msb = read_vram(bank, address + 1);
}

static void get_bg_tile(PpuState *ppu)
{
    uint8_t       scx = io_memory_read(SCX);
    uint8_t       scy = io_memory_read(SCY);
    uint8_t    tile_x = ((scx + (bgw_fetcher->x * TILE_SIZE)) / TILE_SIZE) & LOWER_5_MASK;
    uint8_t    tile_y = ((ly + scy) / TILE_SIZE) % GRID_SIZE;
    uint16_t     base = (lcdc & BIT_3_MASK) ? TM1_ADDRESS_START : TM0_ADDRESS_START;
    uint16_t  address = base + (tile_y * GRID_SIZE) + tile_x;
    bgw_fetcher->tile = read_vram(TILE_MAP_BANK_0, address);
    if (is_gbc()) bgw_fetcher->attr = read_vram(TILE_MAP_BANK_1, address);
}

static void get_bg_tile_data(PpuState *ppu)
{
    uint8_t     scy  = io_memory_read(SCY);
    uint8_t     row  = (ly + scy) % TILE_SIZE;
    uint8_t     bank = TILE_MAP_BANK_0; 

    if (is_gbc())
    {
        bank = (bgw_fetcher->attr & BIT_3_MASK) >> 3;
        row  = (bgw_fetcher->attr & BIT_6_MASK) ? (TILE_SIZE - 1 - row) : row; 
    }

    uint16_t address = get_tile_data_address(lcdc, row);
    bgw_fetcher->lsb = read_vram(bank, address);
    bgw_fetcher->msb = read_vram(bank, address + 1);
}

/* VRAM ACCESS      | -------> OAM SCAN             */

static void oam_scan(Queue *oam_fifo, uint8_t ly)
{
    reset_queue(oam_fifo);
    uint16_t curr_address = OAM_ADDRESS_START;
    uint8_t lcdc = (*ppu->lcd);
    bool stacked = (lcdc & BIT_2_MASK) != 0;

    while(curr_address <= OAM_ADDRESS_END)
    { // TODO: oam_read()?
        uint8_t      y_pos = read_memory(curr_address) - 16;
        bool   on_scanline = (ly - y_pos) < (TILE_SIZE + (stacked * TILE_SIZE));
        if (on_scanline)
        {
            GbcPixel *object     = pixel_schema; // Readability
            uint8_t x_pos        = read_memory(curr_address + 1);
            uint8_t tile_index   = read_memory(curr_address + 2);
            uint8_t attributes   = read_memory(curr_address + 3);
            object->x            = x_pos;
            object->y            = y_pos;
            object->tile_index   = tile_index;
            object->obj_priority = attributes & BIT_7_MASK;
            object->y_flip       = attributes & BIT_6_MASK;
            object->x_flip       = attributes & BIT_5_MASK;
            object->dmg_palette  = attributes & BIT_4_MASK;
            object->bank         = attributes & BIT_3_MASK;
            object->gbc_palette  = (uint8_t) (attributes & LOWER_3_MASK);
            enqueue(oam_fifo, object);
        }
        curr_address += OAM_ENTRY_SIZE;
    }
}

/* ================== FETCHER    ================== */
/* FETCHER          | -------> LOCAL HELP           */
/* FETCHER          | -------> BACKGROUND           */

static bool push_bgw_row()
{
    if (bgw_pix_fifo->size) return false; // Only push if queue is empty.

    uint8_t start = 0;
    if (bgw_fetcher->x != 0)
    {
        start = (*ppu->scx) % TILE_SIZE;
    }

    for (int i = 0; i < TILE_SIZE; i++)
    { 
        bool x_flip = is_gbc() && (bgw_fetcher->attr & BIT_5_MASK);
        uint8_t lsb, msb;
        if (!x_flip)
        {
            lsb = (bgw_fetcher->lsb & BIT_7_MASK) >> 7;
            bgw_fetcher->lsb <<= 1;
            msb = (bgw_fetcher->msb & BIT_7_MASK) >> 7;
            bgw_fetcher->msb <<= 1;
        }
        else
        {
            lsb = bgw_fetcher->lsb & BIT_0_MASK;
            bgw_fetcher->lsb >>= 1;
            msb = bgw_fetcher->msb & BIT_0_MASK;
            bgw_fetcher->msb >>= 1;
        }
        
        if (i < start) continue; // Skip SCX % 8

        uint8_t      color = (msb << 1) | lsb;
        GbcPixel    *pixel = pixel_schema; // Alias for readability
        pixel->   color_id = color;
        pixel->bg_priority = is_gbc() ? (bgw_fetcher->attr & BIT_7_MASK) : 0;
        pixel->gbc_palette = is_gbc() ? (bgw_fetcher->attr & LOWER_3_MASK) : 0;
        enqueue(bgw_pix_fifo, pixel);
    } 
    return true;
}

static void next_bgw_step()
{
    switch (bgw_fetcher->step)
    {  
        case GET_TILE: bgw_fetcher->step =  GET_TDL; break;
        case  GET_TDL: bgw_fetcher->step =  GET_TDH; break;
        case  GET_TDH: bgw_fetcher->step =    SLEEP; break;
        case    SLEEP: bgw_fetcher->step =     PUSH; break;
        case     PUSH: bgw_fetcher->step = GET_TILE; break;
    }
}

static void bgw_fetcher_step(PpuState *ppu)
{
    bgw_fetcher->duration += 1;
    switch(bgw_fetcher->step)
    {
        case GET_TILE:

            if (bgw_fetcher->duration >= 2)
            {
                win_rendering ? 
                get_win_tile(lx, ly, lcdc) : get_bg_tile(ly, lcdc);
                next_bgw_step();
            }

        break;

        case GET_TDL:
            
            if (bgw_fetcher->duration >= 4)
            {
                win_rendering ?
                get_win_tile_data(lx, ly, lcdc) : get_bg_tile_data(ly, lcdc);
                next_bgw_step();
            }

        break;

        case GET_TDH:

            if (bgw_fetcher->duration >= 6)
            {
                bgw_fetcher->row_pushed = push_bgw_row(lx, ly);
                next_bgw_step();
            }

        break;
        
        case SLEEP:

            if (bgw_fetcher->duration >= 8)
            {
                next_bgw_step();
            }
            
        break;

        case PUSH:

            if (!bgw_fetcher->row_pushed)
            {
                bgw_fetcher->row_pushed = push_bgw_row(lx, ly);
            }
            
            if (bgw_fetcher->row_pushed)
            {
                reset_bgw_fetcher();
                fetcher_x += 1;
            }
        
        break;
    }
}

/* FETCHER          | -------> SPRITES              */

static bool push_obj_row()
{ // Assume that pixel has been padded, merge here.
    GbcPixel *target_obj = dequeue(oam_buff);
    for (int i = 0; i < TILE_SIZE; i++)
    { 
        uint8_t lsb, msb;
        if (target_obj->x_flip)
        {
                         lsb = (obj_fetcher->lsb & BIT_7_MASK) >> 7;
            obj_fetcher->lsb = obj_fetcher->lsb << 1;
                         msb = (obj_fetcher->msb & BIT_7_MASK) >> 7;
            obj_fetcher->msb = obj_fetcher->msb << 1;
        }
        else
        {
                         lsb = obj_fetcher->lsb & BIT_0_MASK;
            obj_fetcher->lsb = obj_fetcher->lsb >> 1;
                         msb = obj_fetcher->msb & BIT_0_MASK;
            obj_fetcher->msb = obj_fetcher->msb >> 1;
        }
        uint8_t         color = (msb << 1) | lsb;
        target_obj-> color_id = color; // encode current pixels id. 
        GbcPixel *current_obj = dequeue(obj_pix_fifo);
        enqueue(obj_pix_fifo, get_higher_prio_obj(target_obj, current_obj));
    }
    return true;
}

static GbcPixel *get_higher_prio_obj(GbcPixel *obj1, GbcPixel *obj2)
{
    if (obj1->color_id != 0) return obj2;
    if (obj2->color_id != 0) return obj1;
    if (is_gbc()) // CGB
    {
        if (obj1->oam_address <= obj2->oam_address) // Lower OAM address wins.
        {
            return obj1;
        }
        return obj2;
    }
    else          // DMG
    {
        if (obj1->x <= obj2->x)                     // Smaller x wins.
        {
            return obj1;
        }
        return obj2;
    }
}

static void next_obj_step()
{
    switch(obj_fetcher->step)
    {
        case      GET_TDL: obj_fetcher->step =      GET_TDH; break;
        case      GET_TDH: obj_fetcher->step = PAD_OAM_FIFO; break;
        case PAD_OAM_FIFO: obj_fetcher->step =         PUSH; break;
        case         PUSH: obj_fetcher->step =      GET_TDL; break;
    }
}

static void pad_fifo(Queue *fifo)
{
    GbcPixel *pixel = empty_pixel();
    while(fifo->size != TILE_SIZE)
    {
        enqueue(fifo, pixel);
    } 
}

static void get_obj_tile_data(GbcPixel *obj, PpuState *ppu)
{

}

static void obj_fetcher_step(PpuState *ppu)
{
    obj_fetcher->duration += 1;
    switch(obj_fetcher->step)
    {
        case GET_TDL: 

            if (obj_fetcher->duration >= 2)
            {
                next_obj_step();
            }

        break;

        case GET_TDH:

            if (obj_fetcher->duration >= 4)
            {
                GbcPixel *obj = peek(oam_fifo);
                get_obj_tile_data(obj, ppu);
                next_obj_step();
            }
        
        break;
        
        case PAD_OAM_FIFO: 

            if (obj_fetcher->duration >= 5)
            {
                pad_obj_fifo();
                next_obj_step();
            }   
        
        break;
        
        case PUSH: 

            if (!obj_fetcher->row_pushed)
            {
                obj_fetcher->row_pushed = push_obj_row();
            }
            if (obj_fetcher->row_pushed) reset_obj_fetcher();
        
        break;
    }
}

/* FETCHER         | -------> DRIVER               */

static void pixel_pipeline_step(PpuState *ppu)
{
   

}

static void prep_scanline_render(PpuState *ppu)
{   
    // Normalize PPU State
    reset_ppu(ppu);
    // Empty FIFOs
    reset_queue(obj_pix_fifo);
    reset_queue(bgw_pix_fifo);
    // Reset Fetchers
    reset_fetcher(bgw_fetcher);
    reset_fetcher(obj_fetcher);
}

/* ================== PUBLIC API ================= */

static void set_ppu_mode(PpuMode mode)
{
    (*ppu->stat) = ((*ppu->stat) & ~LOWER_2_MASK) | mode;
}

void dot(uint32_t current_dot)
{
    uint16_t sc_dot = current_dot % DOTS_PER_LINE;
    uint8_t      ly = (uint8_t) (current_dot / DOTS_PER_LINE);
    uint8_t    lcdc = (*ppu->lcdc);
    uint8_t    stat = (*ppu->stat);

    (*ppu->ly) = ly; // Recording scanline position.

    if ((ly < GBC_HEIGHT) && (sc_dot == 0))
    {
        set_ppu_mode(OAM_SCAN);
        oam_scan(oam_fifo, ly);
        bool triggered = (stat & BIT_5_MASK) != 0;
        if (triggered) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    }
    else if ((ly < GBC_HEIGHT) && (sc_dot == 80))
    {
        set_ppu_mode(DRAWING);
        prep_scanline_render(ppu);
    }
    else if ((ly < GBC_HEIGHT) && (sc_dot == 369))
    {
        set_ppu_mode(HBLANK);
        bool triggered = (stat & BIT_4_MASK) != 0;
        if (triggered) request_interrupt(LCD_STAT_INTERRUPT_CODE);
    }
    else if ((ly == GBC_HEIGHT) && (sc_dot == 0))
    {
        set_ppu_mode(VBLANK);
        bool triggered = (stat & BIT_4_MASK) != 0;
        if (triggered) request_interrupt(LCD_STAT_INTERRUPT_CODE);
        request_interrupt(VBLANK_INTERRUPT_CODE); // Happens once per frame, regardless.
    }

    bool drawing = ((stat & LOWER_2_MASK) == DRAWING) && (!ppu->sc_complete);
    if (drawing) pixel_pipeline_step(ppu);

    uint8_t lyc = (*ppu->lyc);
    bool triggered = (ly == lyc) && (stat & BIT_6_MASK);
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

    bgw_fetcher   = (PixelFetcher*) malloc(sizeof(PixelFetcher));
    reset_fetcher(bgw_fetcher); bgw_fetcher->handler = bgw_fetcher_step;
    obj_fetcher   = (PixelFetcher*) malloc(sizeof(PixelFetcher));
    reset_fetcher(obj_fetcher); obj_fetcher->handler = obj_fetcher_step;

    pixel_schema  = (GbcPixel*)     malloc(    sizeof(GbcPixel));
    bgw_pix_fifo  = init_queue(16);
    obj_pix_fifo  = init_queue(8);
    oam_fifo      = init_queue(10);
}

void tidy_graphics()
{
    free(ppu);                   ppu = NULL;
    free(bgw_fetcher);   bgw_fetcher = NULL;
    free(obj_fetcher);   obj_fetcher = NULL;
    free(ppu->lcd);         ppu->lcd = NULL;
    free(pixel_schema); pixel_schema = NULL;

    tidy_queue(bgw_pix_fifo);
    tidy_queue(obj_pix_fifo);
    tidy_queue(oam_fifo);
}

