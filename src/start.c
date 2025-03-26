#include <SDL2/SDL.h>
#include <stdint.h>
#include "common.h"
#include "cpu.h"
#include "emulator.h"
#include "logger.h"
#include "mmu.h"
#include "ppu.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)
#define FRAME_PERIOD 16.74
#define SCALE 4

static SDL_Window        *window;
static SDL_Renderer    *renderer;
static SDL_Texture  *framebuffer;
static bool              running;
static bool      frame_available;
static SDL_mutex          *mutex;
static SDL_cond     *frame_ready;

bool init_display()
{
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        LOG_MESSAGE(ERROR, "SDL did not initialize! %s", SDL_GetError());
        return false;
    }

    // Create SDL Window
    window = SDL_CreateWindow
    (
        "TDog's GBC Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        GBC_WIDTH  * SCALE,
        GBC_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN 
    );
    if(!window)
    {
        LOG_MESSAGE(ERROR, "Could not create Window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // Create SDL Renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        LOG_MESSAGE(ERROR, "Could not create Renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    // Frame buffer Texture
    framebuffer = SDL_CreateTexture
    (
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        GBC_WIDTH,
        GBC_HEIGHT
    );
    if (!framebuffer)
    {
        LOG_MESSAGE(ERROR, "Could not create Texture: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    running = true;
    frame_available = false;
}

void tidy_display()
{
    SDL_DestroyMutex(mutex);
    SDL_DestroyTexture(framebuffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int emu_thread(void *data)
{
    uint8_t cpu_delay = 0;
    uint32_t current_dot = 0;
    while (running)
    {

        if (cpu_running())
        {
            cpu_delay = machine_cycle();
            stop_cpu();
        }
        if (!--cpu_delay) start_cpu();
        
    
        dot(current_dot);
        check_dma();
        current_dot = (current_dot + 1) % DOT_PER_FRAME;

        if (current_dot == 0)
        {
            SDL_LockMutex(mutex);
            while(frame_available)
            {
                SDL_CondWait(frame_ready, mutex);
            }
            frame_available = true;
            SDL_CondSignal(frame_ready);
            SDL_UnlockMutex(mutex);
        }
    }  
}

int main() 
{   
    init_logger(false);
    init_emulator("../roms/btr/cpu_instrs/cpu_instrs.gb", 0x0000);
    //init_emulator("../roms/assembly/graphics_test.gb", 0x0000);
    //init_emulator("../roms/Tetris.gb", 0x0000);
    init_display();
    LOG_MESSAGE(INFO, "Emulator Initialized");
    mutex = SDL_CreateMutex();
    frame_ready = SDL_CreateCond();
    SDL_Thread *emulation_thread = SDL_CreateThread(emu_thread, "Emu Thread", NULL);
    SDL_Event event;
    while(running) 
    {
        Uint64 start_time = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT) 
            {
                running = false;
            }
        }

        SDL_LockMutex(mutex);
        while (!frame_available) 
        {
            SDL_CondWait(frame_ready, mutex);
        }
        frame_available = false;
        SDL_CondSignal(frame_ready);
        SDL_UnlockMutex(mutex);

        SDL_UpdateTexture(framebuffer, NULL, render_frame(), GBC_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, framebuffer, NULL, NULL);
        SDL_RenderPresent(renderer);
        Uint64 perf_freq = SDL_GetPerformanceFrequency();
        Uint64 elapsed = SDL_GetPerformanceCounter() - start_time;
        double elapsed_ms = ((double) elapsed / perf_freq) * 1e3;
        if (elapsed_ms < FRAME_PERIOD)
        {
            SDL_Delay((Uint32)((FRAME_PERIOD - elapsed_ms)));        
        }
    }
    SDL_WaitThread(emulation_thread, NULL);
    tidy_display(); 
    tidy_emulator();
}