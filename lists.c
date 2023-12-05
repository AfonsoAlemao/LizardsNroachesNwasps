#include <stdio.h>
#include <stdlib.h>
# include "lists.h"


// Function to create a new list_element
list_element* createlist_element(square data) {
    list_element* newlist_element = (list_element*)malloc(sizeof(list_element));
    newlist_element->data = data;
    newlist_element->next = NULL;
    return newlist_element;
}

// Function to insert a new list_element at the end
void insertEnd(list_element** head, square data) {
    list_element* newlist_element = createlist_element(data);
    if (*head == NULL) {
        *head = newlist_element;
        return;
    }

    list_element* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newlist_element;
}

// Function to insert a new list_element at the beginning
list_element* insertBegin(list_element** head, square data) {
    list_element* newlist_element = createlist_element(data);
    newlist_element->next = *head;
    *head = newlist_element;

    return *head;
}

// Function to delete a list_element with a given value
list_element* deletelist_element(list_element** head, square data) {
    if (*head == NULL) {
        return NULL;
    }

    list_element* temp = *head;
    list_element* prev = NULL;

    if (compare(temp->data, data) == 0) {
        *head = temp->next;
        free(temp);
        return NULL;
    }

    while (temp != NULL && compare(temp->data, data) != 0) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {
        return NULL;
    }

    prev->next = temp->next;
    free(temp);

    return *head;
}

// Function to push a new list_element onto the stack
void push(list_element** head, square data) {
    insertBegin(head, data);
}

// Function to pop a list_element from the stack
void pop(list_element** head) {
    if (*head == NULL) {
        return;
    }

    list_element* temp = *head;
    *head = temp->next;
    free(temp);
}

// Function to print the linked list
void printList(list_element* head) {
    list_element* temp = head;
    while (temp != NULL) {
        printf("%d ", temp->data);
        temp = temp->next;
    }
    printf("\n");
}

// Function to free the memory allocated for the linked list
void freeList(list_element* head) {
    list_element* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}