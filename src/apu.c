#include <stdint.h>
#include <stdlib.h>
#include "mmu.h"

/*
    Terminology:

    - Envelope: The change of a musical parameter over time.
    -    Sweep: Technique where frequency is systemically varied over a range.
    -  Panning: Send output signal as left/right.
    -      DAC: Digital Analog Converter.
    -      VIN: Voltage Input.
    -      HPF: High Pass Filter.

    Channels:

    - Activated by a write to NRX4.7 (MSB)
    - Deactivated by:
        - Turning of its DAC.
        - Length Timer expiring.
        - Frequency sweep overflow.
*/

typedef struct
{
    uint8_t *nr10; // Sweep
    uint8_t *nr11; // Length Timer & Duty Cycle
    uint8_t *nr12; // Volume & Envelope
    uint8_t *nr13; // Period Low
    uint8_t *nr14; // Period High & Control

} CH1; // Pulse Channel With Sweep

/*

*/

static CH1 *ch1;

typedef struct
{
    uint8_t *nr21; // Length Timer & Duty Cycle
    uint8_t *nr22; // Volume and Envelope
    uint8_t *nr23; // Period Low
    uint8_t *nr24; // Period High & Control

} CH2; // Pulse Channel

/*
    
*/

static CH2 *ch2;

typedef struct
{
    uint8_t *nr30; // DAC Enable
    uint8_t *nr31; // Length Timer
    uint8_t *nr32; // Output Level
    uint8_t *nr33; // Period Low
    uint8_t *nr34; // Period High & Control

} CH3; // Wave Channel

static CH3 *ch3;

typedef struct
{
    uint8_t *nr41; // Length Timer
    uint8_t *nr42; // Volume & Envelope
    uint8_t *nr43; // Frequency & Randomness
    uint8_t *nr44; // Control

} CH4; // Noise Channel

/*
    
*/

static CH4 *ch4;

typedef struct
{
    uint8_t *nr50; // Master Volume & VIN Panning
    uint8_t *nr51; // Sound Panning
    uint8_t *nr52; // Sound On/Off

} Apu;

static Apu *apu;

init_apu()
{
    ch1 = (CH1*) malloc(sizeof(CH1));
    ch1->nr10 = get_memory_pointer(NR10);
    ch1->nr11 = get_memory_pointer(NR11);
    ch1->nr12 = get_memory_pointer(NR12);
    ch1->nr13 = get_memory_pointer(NR13);
    ch1->nr14 = get_memory_pointer(NR14);

    ch2 = (CH2*) malloc(sizeof(CH2));
    ch2->nr21 = get_memory_pointer(NR21);
    ch2->nr22 = get_memory_pointer(NR22);
    ch2->nr23 = get_memory_pointer(NR23);
    ch2->nr24 = get_memory_pointer(NR24);

    ch3 = (CH3*) malloc(sizeof(CH3));
    ch3->nr30 = get_memory_pointer(NR30);
    ch3->nr31 = get_memory_pointer(NR31);
    ch3->nr32 = get_memory_pointer(NR32);
    ch3->nr33 = get_memory_pointer(NR33);
    ch3->nr34 = get_memory_pointer(NR34);

    ch4 = (CH4*) malloc(sizeof(CH4));
    ch4->nr41 = get_memory_pointer(NR41);
    ch4->nr42 = get_memory_pointer(NR42);
    ch4->nr43 = get_memory_pointer(NR43);
    ch4->nr44 = get_memory_pointer(NR44);

    apu = (Apu*) malloc(sizeof(Apu));
    apu->nr50 = get_memory_pointer(NR50);
    apu->nr51 = get_memory_pointer(NR51);
    apu->nr52 = get_memory_pointer(NR52);
}

tidy_apu()
{
    free(ch1); ch1 = NULL;
    free(ch2); ch2 = NULL; 
    free(ch3); ch3 = NULL;
    free(ch4); ch4 = NULL;
    free(apu); apu = NULL;
}