#ifndef fifo_H
#define fifo_H

#include <stdbool.h>
#include <stdint.h>

/* Stores a dead roach */
typedef struct dead_roach 
{
    int index_client;  /* Roach stored in roachtable[index_client].char_data[index_roaches] */
    int index_roaches;  
    int64_t death_time;   /* Death time */
}dead_roach;

/* Define a fifo_element structure in order to store a fifo of dead roaches */
typedef struct fifo_element {
    dead_roach data;
    struct fifo_element* next;
}fifo_element;

/* Function to create a new fifo_element */
fifo_element* createfifo_element(dead_roach data);

/* Function to insert a new fifo_element at the end */
bool insertEnd_fifo(fifo_element** head, dead_roach data);

/* Function to push a new fifo_element onto the stack */
bool push_fifo(fifo_element** head, dead_roach data);

/* Function to pop a fifo_element from the stack */
void pop_fifo(fifo_element** head);

/* Function to print the fifo */
void printfifo(fifo_element* head);

/* Function to free the memory allocated for the fifo */
void freefifo(fifo_element* head);

#endif