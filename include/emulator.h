#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    A_BUTTON_MASK      = 0b00000001,
    B_BUTTON_MASK      = 0b00000010,
    SELECT_BUTTON_MASK = 0b00000100,
    START_BUTTON_MASK  = 0b00001000,
    DOWN_BUTTON_MASK   = 0b00001000,
    UP_BUTTON_MASK     = 0b00000100,
    LEFT_BUTTON_MASK   = 0b00000010,
    RIGHT_BUTTON_MASK  = 0b00000001

} JoypadMask;

typedef struct
{
    bool     A, B, SELECT, START;
    bool     RIGHT, LEFT, UP, DOWN;

    uint8_t turbo_scaler;
    bool   turbo_enabled;

} JoypadState;

JoypadState *get_joypad();

void init_emulator(char *file_path, bool display);

void tidy_emulator(bool reset_display);

void start_emulator();

void stop_emulator();

char *get_joypad_state(char *buffer, uint8_t size);

#endif