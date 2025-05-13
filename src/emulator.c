#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "cart.h"
#include "common.h"
#include "cpu.h"
#include "emulator.h"
#include "logger.h"
#include "mmu.h"
#include "ppu.h"
#include "timer.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)
#define DEFAULT_TURBO 4
#define FRAME_PERIOD 16.74
#define SCALE 4

static SDL_Window        *window;
static SDL_Renderer    *renderer;
static SDL_Texture  *framebuffer;
static bool              running;
static bool      frame_available;
static SDL_mutex          *mutex;
static SDL_cond     *frame_ready;
static char      *cartridge_file;

JoypadState *joypad;

static bool init_display()
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

static void tidy_display()
{
    SDL_DestroyMutex(mutex);
    SDL_DestroyTexture(framebuffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static void init_joypad()
{
    joypad = (JoypadState*) malloc(sizeof(JoypadState));
    joypad->     A = false;
    joypad->     B = false;
    joypad->SELECT = false;
    joypad-> START = false;
    joypad->    UP = false;
    joypad->  DOWN = false;
    joypad-> RIGHT = false;
    joypad->  LEFT = false;

    joypad->turbo_enabled = false;
    joypad-> turbo_scaler = DEFAULT_TURBO;
}

static void tidy_joypad()
{
    free(joypad); joypad = NULL;
}

void init_emulator(char *file_path, bool display)
{
    init_memory();
    LOG_MESSAGE(INFO, "Memory initialized.");
    init_timer();
    LOG_MESSAGE(INFO, "Timer initialized.");
    init_cartridge(file_path);
    LOG_MESSAGE(INFO, "Cartridge initialized.");
    init_cpu();
    LOG_MESSAGE(INFO, "CPU initialized.");
    init_graphics();
    LOG_MESSAGE(INFO, "Graphics initialized.");

    if (display)
    {
        init_display();
        LOG_MESSAGE(INFO, "Display initialized.");

        init_joypad();
        LOG_MESSAGE(INFO, "Joypad, locked and loaded!");
    }

    cartridge_file = file_path;
}

void tidy_emulator(bool display)
{
    tidy_memory();
    tidy_timer();
    tidy_cartridge();
    tidy_cpu();
    tidy_graphics();
    if (display) 
    {
        tidy_display();
        tidy_joypad();
    }
}

int emu_thread(void *data)
{
    while (running)
    {
        uint32_t dot = system_clock_pulse();
        if (dot == 0)
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

char *get_joypad_state(char *buffer, uint8_t size)
{
    snprintf
    (buffer,
     size,
     "[(A - %d) (B - %d) (SEL - %d) (START - %d) || (R - %d) (L - %d) (U - %d) (D - %d)]",
     joypad->A,
     joypad->B,
     joypad->SELECT,
     joypad->START,
     joypad->RIGHT,
     joypad->LEFT,
     joypad->UP,
     joypad->DOWN
    );
    return buffer;
}

static void reset_emulator()
{
    if (joypad->turbo_enabled) return;
    tidy_emulator(false);
    init_emulator(cartridge_file, false);
}

static void increment_turbo(JoypadState *joypad)
{
    joypad->turbo_scaler += 1;
    if (joypad->turbo_scaler > 10) 
    {
        joypad->turbo_scaler = 10;
    }
}

static void decrement_turbo(JoypadState *joypad)
{
    joypad->turbo_scaler -= 1;
    if (joypad->turbo_scaler < 1) 
    {
        joypad->turbo_scaler = 1;
    } 
}

static void handle_button_press(SDL_Event *event)
{
    if (!joypad || event->key.repeat) return;

    SDL_Keycode key = event->key.keysym.sym;

    switch (key)
    {
        case SDLK_x:         joypad->            A = true; break;
        case SDLK_z:         joypad->            B = true; break;
        case SDLK_RETURN:    joypad->        START = true; break;
        case SDLK_BACKSPACE: joypad->       SELECT = true; break;
        case SDLK_UP:        joypad->           UP = true; break;
        case SDLK_DOWN:      joypad->         DOWN = true; break;
        case SDLK_RIGHT:     joypad->        RIGHT = true; break;
        case SDLK_LEFT:      joypad->         LEFT = true; break;
        case SDLK_SPACE:     joypad->turbo_enabled = true; break;
    }
}

static void handle_button_release(SDL_Event *event)
{
    if (!joypad) return;

    SDL_Keycode key = event->key.keysym.sym;

    switch (key)
    {
        case SDLK_p:         increment_turbo(joypad);       break;
        case SDLK_o:         decrement_turbo(joypad);       break;
        case SDLK_r:         reset_emulator();              break;
        case SDLK_x:         joypad->            A = false; break;
        case SDLK_z:         joypad->            B = false; break;
        case SDLK_RETURN:    joypad->        START = false; break;
        case SDLK_BACKSPACE: joypad->       SELECT = false; break;
        case SDLK_UP:        joypad->           UP = false; break;
        case SDLK_DOWN:      joypad->         DOWN = false; break;
        case SDLK_RIGHT:     joypad->        RIGHT = false; break;
        case SDLK_LEFT:      joypad->         LEFT = false; break;
        case SDLK_SPACE:     joypad->turbo_enabled = false; break;
    }
}

static void handle_events()
{
    SDL_Event event;
    bool jirn = false; // Joypad Interrupt Request Needed

    while (SDL_PollEvent(&event)) // Keep going until event queue is empty.
    {
        switch(event.type)
        {
            case SDL_QUIT:
                running = false;
                break;
            
            case SDL_KEYDOWN:
                handle_button_press(&event);
                jirn = true;
                break;
            
            case SDL_KEYUP:
                handle_button_release(&event);
                jirn = true;
                break;
        }
    }
    
    if (jirn) 
    {
        request_interrupt(JOYPAD_INTERRUPT_CODE);
    }
}

JoypadState *get_joypad()
{
    return joypad;
}

void start_emulator()
{
    running = true;
    mutex = SDL_CreateMutex();
    frame_ready = SDL_CreateCond();
    SDL_Thread *emulation_thread = SDL_CreateThread(emu_thread, "Emu Thread", NULL);
    while(running) // 1 Loop = 1 Frame
    {
        Uint64 start_time = SDL_GetPerformanceCounter();
        // Record input into joypad.
        handle_events();
        // Check if frame is ready.
        SDL_LockMutex(mutex);
        while (!frame_available) 
        {
            SDL_CondWait(frame_ready, mutex);
        }
        frame_available = false;
        SDL_CondSignal(frame_ready);
        SDL_UnlockMutex(mutex);
        
        // Frame ready, good to go.
        // Consume frame and update texture to render.
        SDL_UpdateTexture(framebuffer, NULL, render_frame(), GBC_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, framebuffer, NULL, NULL);
        SDL_RenderPresent(renderer);

        // Calculate and provide frame delay.
        Uint64 perf_freq = SDL_GetPerformanceFrequency();
        Uint64 elapsed = SDL_GetPerformanceCounter() - start_time;
        double elapsed_ms = ((double) elapsed / perf_freq) * 1e3;
        if (elapsed_ms < FRAME_PERIOD)
        {
            uint32_t delay = (Uint32)(FRAME_PERIOD - elapsed_ms);
            delay = joypad->turbo_enabled ? (delay / joypad->turbo_scaler) : delay;
            SDL_Delay(delay);        
        }
    }
    SDL_WaitThread(emulation_thread, NULL);
}
