#ifndef UTIL_H
#define UTIL_H

typedef struct
{
    uint16_t oam_address;
    uint8_t     color_id;
    uint8_t  gbc_palette;
    uint8_t            x;
    uint8_t            y;
    uint8_t   tile_index;
    uint8_t         bank;
    uint8_t  dmg_palette;
    bool          is_obj;
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

GbcPixel *peek(Queue *q);

GbcPixel *dequeue(Queue *q);

void reset_queue();

void sort_oam_by_xpos(Queue *objs);

uint8_t queue_size(Queue *queue);

void print_queue(Queue *queue);

#endif
