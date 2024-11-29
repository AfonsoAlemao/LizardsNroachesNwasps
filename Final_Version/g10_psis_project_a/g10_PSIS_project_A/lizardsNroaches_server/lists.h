#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdbool.h>

typedef struct square 
{
    int index_client;  /* Index in table of clients with roaches or in table of lizards  */
    int index_roaches;  /* Index in table of the client of the roaches. Only used for roaches */
    int element_type;   /* 0-tail of lizard, 1-lizard head, 2-roach */

    /* Examples:
    lizard stored in lizardtable[index_client]: 
        index_roaches=don't care
        element_type=1

    tail of lizard stored in lizardtable[index_client]: 
        index_roaches=don't care
        element_type=0

    roach stored in roachtable[index_client].char_data[index_roaches]: 
        element_type=2
    */
}square;


/* Define a list_element structure in order to store a list of elements in a field position */
typedef struct list_element {
    square data;
    struct list_element* next;
}list_element;

/* Function to create a new list_element */
list_element* createlist_element(square data);

/* Function to insert a new list_element at the beginning */
bool insertBegin(list_element** head, square data);

/* Function to delete a list_element with a given value */
void deletelist_element(list_element** head, square data);

/* Function to print the linked list */
void printList(list_element* head);

/* Function to free the memory allocated for the linked list */
void freeList(list_element* head);

#endif