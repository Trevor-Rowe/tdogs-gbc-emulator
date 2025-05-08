#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdbool.h>
#include <stdint.h>
/*
    Initializes emulator and all its dependencies.
    @param file_path: location of file to read in.
    @param testing  : flag for extra debug info.
    @note: CALL THIS FIRST.
*/
void init_emulator(char *file_path, uint16_t entry);

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

} JoypadState;

JoypadState *get_joypad();

void tidy_emulator();

void start_emulator();

void stop_emulator();

char *get_joypad_state(char *buffer, uint8_t size);

#endif