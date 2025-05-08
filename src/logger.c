#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "emulator.h"
#include "logger.h"
#include "mmu.h"
#include "cpu.h"
#include "timer.h"

static bool override = false;

void log_message
(
    LoggingLevel level,
    const char *file, 
    const char *func, 
    const char *format, 
    ...
) 
{
    if (level == TEST || level == INFO || level == ERROR || override)
    {
        // Log level string
        const char *level_str;
        switch (level) {
            case INFO:    level_str = "--INFO----"; break;
            case WARNING: level_str = "--WARNING-"; break;
            case ERROR:   level_str = "--ERROR---"; break;
            case DEBUG:   level_str = "--DEBUG---"; break;
            case TEST:    level_str = "--TEST----"; break;
            default:      level_str = "--LOG-----"; break;
        }

        int buffer_size = snprintf(NULL, 0, "\n[%-8s] | %-18s | %-15s |", level_str, file, func) + 1;
        char formatted_string[buffer_size];
        snprintf(formatted_string, buffer_size, "\n[%-8s] | %-18s | %-15s |", level_str, file, func);
        printf("%s ", formatted_string);
    
        va_list args; // Consume variadtic args
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

void cpu_log(LoggingLevel level, const char *message, ...)
{
    if (level == TEST || level == INFO || level == ERROR || override)
    {
        const char *level_str;
        switch (level) {
            case INFO:    level_str = "--INFO----"; break;
            case WARNING: level_str = "--WARNING-"; break;
            case ERROR:   level_str = "--ERROR---"; break;
            case DEBUG:   level_str = "--DEBUG---"; break;
            case TEST:    level_str = "--TEST----"; break;
            default:      level_str = "--LOG-----"; break;
        }

        char time_buffer[50]; 
        const char  *time = get_emu_time(time_buffer, sizeof(time_buffer));
    
        char state_buffer[100];
        const char *state = get_cpu_state(state_buffer, sizeof(state_buffer));
    
        printf("\n[%s] | %s | %s ", level_str, time, state);

        va_list args;
        va_start(args, message);
        vprintf(message, args);
        va_end(args);
    }
}

void joypad_log(LoggingLevel level, const char *message, ...)
{
    if (level == TEST || level == INFO || level == ERROR || override)
    {
        const char *level_str;
        switch (level) {
            case INFO:    level_str = "--INFO----"; break;
            case WARNING: level_str = "--WARNING-"; break;
            case ERROR:   level_str = "--ERROR---"; break;
            case DEBUG:   level_str = "--DEBUG---"; break;
            case TEST:    level_str = "--TEST----"; break;
            default:      level_str = "--LOG-----"; break;
        }
    
        char buffer[100];
        const char *state = get_joypad_state(buffer, sizeof(buffer));
    
        printf("\n[%s] %s ", level_str, state);

        va_list args;
        va_start(args, message);
        vprintf(message, args);
        va_end(args);
    }
}