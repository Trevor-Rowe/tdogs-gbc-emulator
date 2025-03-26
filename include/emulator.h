#ifndef EMULATOR_H
#define EMULATOR_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*
    Initializes emulator and all its dependencies.
    @param file_path: location of file to read in.
    @param testing  : flag for extra debug info.
    @note: CALL THIS FIRST.
*/
void init_emulator(char *file_path, uint16_t entry);

/*
    Stops emulator and tidies up.
*/
void tidy_emulator();

int start_emulator(void *data);

void stop_emulator();

#endif