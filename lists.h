#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdbool.h>

typedef struct square 
{
    int index_client;  // index in table of clients with roaches or in table of lizards 
    int index_roaches;  // index in table of the client of the roaches. Only used for roaches
    int element_type;   // 0-tail of liz, 1-liz head, 2-roach

    /* Examples:
    lizard stored in lizardtable[0]: 
        index_client=0
        index_roaches=don't care
        element_type=1

    tail of lizard stored in lizardtable[0]: 
        index_client=0
        index_roaches=don't care
        element_type=0

    roach stored in roachtable[0][3]: 
        index_client=0
        index_roaches=3
        element_type=2
    */
}square;


// Define a list_element structure
typedef struct list_element {
    square data;
    struct list_element* next;
}list_element;

// Function to create a new list_element
list_element* createlist_element(square data);

// Function to insert a new list_element at the beginning
bool insertBegin(list_element** head, square data);

// Function to delete a list_element with a given value
void deletelist_element(list_element** head, square data);

// Function to print the linked list
void printList(list_element* head);

// Function to free the memory allocated for the linked list
void freeList(list_element* head);

#endif