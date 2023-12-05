#include <stdio.h>
#include <stdlib.h>
# include "lists.h"

// Function to create a new square
struct square* createsquare(int data) {
    struct square* newsquare = (struct square*)malloc(sizeof(struct square));
    newsquare->data = data;
    newsquare->next = NULL;
    return newsquare;
}

// Function to insert a new square at the end
void insertEnd(struct square** head, int data) {
    struct square* newsquare = createsquare(data);
    if (*head == NULL) {
        *head = newsquare;
        return;
    }

    struct square* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newsquare;
}

// Function to insert a new square at the beginning
void insertBegin(struct square** head, int data) {
    struct square* newsquare = createsquare(data);
    newsquare->next = *head;
    *head = newsquare;
}

// Function to delete a square with a given value
void deletesquare(struct square** head, int data) {
    if (*head == NULL) {
        return;
    }

    struct square* temp = *head;
    struct square* prev = NULL;

    if (temp->data == data) {
        *head = temp->next;
        free(temp);
        return;
    }

    while (temp != NULL && temp->data != data) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {
        return;
    }

    prev->next = temp->next;
    free(temp);
}

// Function to push a new square onto the stack
void push(struct square** head, int data) {
    insertBegin(head, data);
}

// Function to pop a square from the stack
void pop(struct square** head) {
    if (*head == NULL) {
        return;
    }

    struct square* temp = *head;
    *head = temp->next;
    free(temp);
}

// Function to print the linked list
void printList(struct square* head) {
    struct square* temp = head;
    while (temp != NULL) {
        printf("%d ", temp->data);
        temp = temp->next;
    }
    printf("\n");
}

// Function to free the memory allocated for the linked list
void freeList(struct square* head) {
    struct square* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}