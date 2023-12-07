#ifndef fifo_H
#define fifo_H

typedef struct dead_roach 
{
    int index_client;  // index in table of clients with roaches 
    int index_roaches;  // index in table of the client of the roaches. Only used for roaches
    double death_time;   // death time 

    /* Examples:

    roach stored in roachtable[0][3]: 
        index_client=0
        index_roaches=3
    */
}dead_roach;

// Define a fifo_element structure
typedef struct fifo_element {
    dead_roach data;
    struct fifo_element* next;
}fifo_element;

// Function to create a new fifo_element
fifo_element* createfifo_element(dead_roach data);

// Function to insert a new fifo_element at the end
void insertEnd_fifo(fifo_element** head, dead_roach data);

// Function to push a new fifo_element onto the stack
void push_fifo(fifo_element** head, dead_roach data);

// Function to pop a fifo_element from the stack
void pop_fifo(fifo_element** head);

// Function to print the fifo
void printfifo(fifo_element* head);

// Function to free the memory allocated for the fifo
void freefifo(fifo_element* head);

#endif