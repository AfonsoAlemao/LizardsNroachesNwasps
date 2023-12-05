#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include "remote-char.h"

// Define a list_element structure
typedef struct list_element {
    square data;
    list_element* next;
}list_element;

// Function to create a new list_element
list_element* createlist_element(square data);

// Function to insert a new list_element at the end
void insertEnd(list_element** head, square data);

// Function to insert a new list_element at the beginning
void insertBegin(list_element** head, square data);

// Function to delete a list_element with a given value
void deletelist_element(list_element** head, square data);

// Function to push a new list_element onto the stack
void push(list_element** head, square data);

// Function to pop a list_element from the stack
void pop(list_element** head);

// Function to print the linked list
void printList(list_element* head);

// Function to free the memory allocated for the linked list
void freeList(list_element* head);

#endif