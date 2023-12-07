#include <stdio.h>
#include <stdlib.h>
#include "lists.h"

void *free_safe2 (void *aux) {
    if (aux != NULL) {
        free(aux);
    }
    return NULL;
}

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
void insertBegin(list_element** head, square data) {
    list_element* newlist_element = createlist_element(data);
    newlist_element->next = *head;
    *head = newlist_element;

    return;
}

int compare(square data1, square data2) {
    if (data1.element_type == data2.element_type &&
        data1.index_client == data2.index_client &&
        data1.index_roaches == data2.index_roaches) {
        return 0;
    }
    return 1;
}



void deletelist_element(list_element** head, square data) {
    if (*head == NULL) {
        return;
    }

    list_element* temp = *head;
    list_element* prev = NULL;

    if (compare(temp->data, data) == 0) {
        *head = temp->next;
        temp = free_safe2(temp);
        return;
    }

    while (temp != NULL && compare(temp->data, data) != 0) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {
        return;
    }

    prev->next = temp->next;
    temp = free_safe2(temp);

    return;
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
    temp = free_safe2(temp);
}

// Function to print the linked list
void printList(list_element* head) {
    list_element* temp = head;
    while (temp != NULL) {
        printf("Element Type %d ", temp->data.element_type);
        printf("Index Client %d ", temp->data.index_client);
        printf("Index Roaches %d ", temp->data.index_roaches);
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
        temp = free_safe2(temp);
    }
}
