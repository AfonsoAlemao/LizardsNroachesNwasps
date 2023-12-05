#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include "remote-char.h"

// Define a square structure
struct square {
    list_element data;
    struct square* next;
};

// Function to create a new square
struct square* createsquare(int data);

// Function to insert a new square at the end
void insertEnd(struct square** head, int data);

// Function to insert a new square at the beginning
void insertBegin(struct square** head, int data);

// Function to delete a square with a given value
void deletesquare(struct square** head, int data);

// Function to push a new square onto the stack
void push(struct square** head, int data);

// Function to pop a square from the stack
void pop(struct square** head);

// Function to print the linked list
void printList(struct square* head);

// Function to free the memory allocated for the linked list
void freeList(struct square* head);

#endif