#include <stdio.h>
#include <stdlib.h>
#include "fifo.h"

// Function to create a new fifo_element
fifo_element* createfifo_element(dead_roach data) {
    fifo_element* newfifo_element = (fifo_element*)malloc(sizeof(fifo_element));
    newfifo_element->data = data;
    newfifo_element->next = NULL;
    return newfifo_element;
}

// Function to insert a new fifo_element at the end
void insertEnd_fifo(fifo_element** head, dead_roach data) {
    fifo_element* newfifo_element = createfifo_element(data);
    if (*head == NULL) {
        *head = newfifo_element;
        return;
    }

    fifo_element* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newfifo_element;
}

int compare_fifo(dead_roach data1, dead_roach data2) {
    if (data1.death_time != data2.death_time ||
        data1.index_client != data2.index_client ||
        data1.index_roaches != data2.index_roaches) {
        return 1;
    }
    return 0;
}

// Function to push a new fifo_element onto the stack
void push_fifo(fifo_element** head, dead_roach data) {
    insertEnd_fifo(head, data);
}


// Function to pop a fifo_element from the stack
fifo_element *pop_fifo(fifo_element** head) {
    if (*head == NULL) {
        return NULL;
    }
    fifo_element* temp = *head;
    *head = temp->next;
    free(temp);

    return *head;
}

// Function to print the fifo
void printfifo(fifo_element* head) {
    fifo_element* temp = head;
    while (temp != NULL) {
        printf("Death time %lf ", temp->data.death_time);
        printf("Index Client %d ", temp->data.index_client);
        printf("Index Roaches %d ", temp->data.index_roaches);
        temp = temp->next;
    }
    printf("\n");
}

// Function to free the memory allocated for the fifo
void freefifo(fifo_element* head) {
    fifo_element* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}
