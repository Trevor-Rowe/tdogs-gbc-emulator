#ifndef EMULATOR_H
#define EMULATOR_H
#include <stdbool.h>
#include <stdio.h>

typedef struct
{
    bool running;

} EmulatorState;

/*
    Initializes emulator and all its dependencies.
    @param file_path: location of file to read in.
    @param testing  : flag for extra debug info.
    @note: CALL THIS FIRST.
*/
void init_emulator(char *file_path, bool testing);

/* 
    Starts program execution.
    @note: CALL THIS SECOND.
*/
void start_emulator();

/*
    Stops emulator and tidies up.
*/
void tidy_emulator();

#endif