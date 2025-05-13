#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "logger.h"
#include "util.h"

#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

/* CIRCULAR QUEUE IMPLEMENTATION FOR PIXEL FETCHER */

Queue *init_queue(uint16_t capacity)
{
    Queue *queue    = (Queue*) malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->items    = (GbcPixel**) malloc(capacity * sizeof(GbcPixel*));
    queue->front    = -1;
    queue->rear     = -1;
    queue->size     =  0;
    for (uint8_t i = 0; i < capacity; i++)
    {
        queue->items[i] = (GbcPixel*) malloc(sizeof(GbcPixel));
    }
    return queue;
}

void tidy_queue(Queue *queue)
{
    for (uint8_t i = 0; i < queue->capacity; i++)
    {
        free(queue->items[i]);
    }
    free(queue->items);
    queue->items = NULL;
    free(queue);
    queue = NULL;
}

bool is_full(Queue *queue)
{
    return queue->size == queue->capacity;
}

bool is_empty(Queue *queue)
{
    return queue->size == 0;
}

void enqueue(Queue *queue, GbcPixel *value)
{
    if (is_full(queue)) return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    GbcPixel *temp = queue->items[queue->rear];
    temp-> oam_address = value->oam_address;
    temp->    color_id = value->color_id;
    temp-> gbc_palette = value->gbc_palette;
    temp->           x = value->x;
    temp->           y = value->y;
    temp->  tile_index = value->tile_index;
    temp-> dmg_palette = value->dmg_palette;
    temp->        bank = value->bank;
    temp->      is_obj = value->is_obj;
    temp->obj_priority = value->obj_priority;
    temp-> bg_priority = value->bg_priority;
    temp->      x_flip = value->x_flip;
    temp->      y_flip = value->y_flip;
    if (queue->front == -1) queue->front = 0;
    queue->size++;
}

void reset_queue(Queue *queue)
{
    queue->front    = -1;
    queue->rear     = -1;
    queue->size     =  0; 
}

GbcPixel *peek(Queue *queue)
{
    return queue->items[0];
}

GbcPixel *dequeue(Queue *queue)
{
    if (is_empty(queue)) return NULL;
    GbcPixel *value = queue->items[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return value;
}

void sort_oam_by_xpos(Queue *objs)
{
    if (objs->size < 2) return;
    uint8_t index = 1;
    while(index < objs->size)
    {
        if (index == 0) index = 1;
        uint8_t x_prev = (objs->items[index - 1])->x;
        uint8_t x_curr = (objs->items[index])->x;
        if (x_prev > x_curr)
        {
            GbcPixel *obj_prev = objs->items[index - 1]; 
            GbcPixel *obj_curr = objs->items[index];
            // SWAP
            objs->items[index - 1] = obj_curr;
            objs->items[index] = obj_prev;
            index -= 1;
            continue;
        }
        index += 1;
    }
}

uint8_t queue_size(Queue *queue)
{
    return queue->size;
}

void print_queue(Queue *queue)
{
    for (int i = 0; i < queue->size; i++)
    {
        GbcPixel **items = queue->items;
        LOG_MESSAGE(TEST, "%d", items[i]->x);
    }
    LOG_MESSAGE(TEST, "");
}
