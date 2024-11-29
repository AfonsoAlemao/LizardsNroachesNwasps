#include <stdio.h>
#include <stdlib.h>
#include "lists.h"

void *free_safe2 (void *aux) {
    if (aux != NULL) {
        free(aux);
    }
    return NULL;
}

/* Function to create a new list_element */
list_element* createlist_element(square data) {
    list_element* newlist_element = (list_element*)malloc(sizeof(list_element));
    if (newlist_element == NULL) {
        printf("Error creating a new list_element.\n");
        return NULL;
    }
    newlist_element->data = data;
    newlist_element->next = NULL;
    return newlist_element;
}


/* Function to insert a new list_element at the beginning */
bool insertBegin(list_element** head, square data) {
    list_element* newlist_element = createlist_element(data);
    if (newlist_element == NULL) {
        return false;
    }
    
    newlist_element->next = *head;
    *head = newlist_element;

    return true;
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

/* Function to print the linked list */
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

/* Function to free the memory allocated for the linked list */
void freeList(list_element* head) {
    list_element* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        temp = free_safe2(temp);
    }
}
