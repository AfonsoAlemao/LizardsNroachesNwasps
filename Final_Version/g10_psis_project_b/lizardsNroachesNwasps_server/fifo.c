#include <stdio.h>
#include <stdlib.h>
#include "fifo.h"

void *free_safe3 (void *aux) {
    if (aux != NULL) {
        free(aux);
    }
    return NULL;
}

/* Function to create a new fifo_element */
fifo_element* createfifo_element(dead_roach data) {
    fifo_element* newfifo_element = (fifo_element*)malloc(sizeof(fifo_element));
    if (newfifo_element == NULL) {
        printf("Error creating a new fifo_element.\n");
        return NULL;
    }
    newfifo_element->data = data;
    newfifo_element->next = NULL;
    return newfifo_element;
}

/* Function to insert a new fifo_element at the end */
bool insertEnd_fifo(fifo_element** head, dead_roach data) {
    fifo_element* newfifo_element = createfifo_element(data);
    if (newfifo_element == NULL) {
        printf("Error inserting fifo_element at end.\n");
        return false;
    }
    if (*head == NULL) {
        *head = newfifo_element;
        return true;
    }

    fifo_element* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newfifo_element;
    return true;
}

int compare_fifo(dead_roach data1, dead_roach data2) {
    if (data1.death_time == data2.death_time &&
        data1.index_client == data2.index_client &&
        data1.index_bot == data2.index_bot) {
        return 0;
    }
    return 1;
}

/* Function to push a new fifo_element onto the stack */
bool push_fifo(fifo_element** head, dead_roach data) {
    if (insertEnd_fifo(head, data)) {
        return true;
    }
    return false;
}


/* Function to pop a fifo_element from the stack */
void pop_fifo(fifo_element** head) {
    if (*head == NULL) {
        return;
    }
    fifo_element* temp = *head;
    *head = temp->next;
    temp = free_safe3(temp);

    return;
}

/* Function to print the fifo */
void printfifo(fifo_element* head) {
    fifo_element* temp = head;
    while (temp != NULL) {
        printf("Death time %ld ", temp->data.death_time);
        printf("Index Client %d ", temp->data.index_client);
        printf("Index Roaches %d ", temp->data.index_bot);
        temp = temp->next;
    }
    printf("\n");
}

/* Function to free the memory allocated for the fifo */
void freefifo(fifo_element* head) {
    fifo_element* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        temp = free_safe3(temp);
    }
}
