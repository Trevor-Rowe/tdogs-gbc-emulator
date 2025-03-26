#ifndef GBC_UTIL_H
#define GBC_UTIL_H

#include "stdbool.h"
#include "stdint.h"

typedef struct
{
    uint16_t oam_address;
    uint8_t     color_id;
    uint8_t  gbc_palette;
    uint8_t        x_obj;
    uint8_t        y_obj;
    uint8_t       x_draw;
    uint8_t       y_draw;
    uint8_t   tile_index;
    uint8_t         bank;
    uint8_t  dmg_palette; 
    bool    obj_priority;
    bool     bg_priority;   
    bool          x_flip;
    bool          y_flip;

} GbcPixel; 

typedef struct
{
    GbcPixel **items;
    int        front;
    int         rear;
    int         size;
    int     capacity;

} Queue;

Queue *init_queue(uint16_t capacity);

void tidy_queue(Queue *q); 

bool is_full(Queue *q);

bool is_empty(Queue *q);

void enqueue(Queue *q, GbcPixel *value);

GbcPixel *dequeue(Queue *q);

void reset_queue();

void sort_oam_by_xpos(Queue *objs);

uint8_t queue_size(Queue *queue);

#endif
