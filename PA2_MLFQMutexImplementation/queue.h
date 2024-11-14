#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <vector>
#include <string> 
#include <fstream> 
#include "park.h"

#ifndef QUEUE_H
#define QUEUE_H

void logMessage(const string &message) {
    ofstream logFile("log.txt");
    logFile << message << endl;
    logFile.close();
}

using namespace std;

template <class T>
struct Node {
    T value; // Value contained in the node
    Node *next; // Pointer to the next node in the queue

    // Default constructor
    Node() 
        : next(nullptr) {}
    // Parameterized constructor
    Node(T givenValue, Node *givenNext = nullptr)
        : value(givenValue), next(givenNext) {}
};

template <class T>
class Queue {

    public:
    
    // Default constructor
    Queue() {
        Node<T> *dummyNode = new Node<T>(); // Create a dummy node
        head = dummyNode; // Head points to dummy node
        tail = dummyNode; // Tail also points to dummy node
        pthread_mutex_init(&head_lock, NULL); // Initialize head mutex
        pthread_mutex_init(&tail_lock, NULL); // Initialize tail mutex
    }

    // Destructor
    ~Queue() {
        while (head != nullptr) {
            Node<T>* temp = head;
            head = head->next;
        }
        // Destroy mutexes
        pthread_mutex_destroy(&head_lock);
        pthread_mutex_destroy(&tail_lock);
    }

    // Enqueue method
    void enqueue(T item) {
        Node<T> *newNode = new Node<T>(item); // Create new node

        pthread_mutex_lock(&tail_lock); // Lock tail mutex

        tail->next = newNode; // Link last node to new node
        tail = newNode; // Update tail to new node

        pthread_mutex_unlock(&tail_lock); // Unlock tail mutex
    }

    // Dequeue method
    T dequeue() {
        pthread_mutex_lock(&head_lock); // Lock head mutex

        Node<T> *dummyNode = head;
        Node<T> *newDummyNode = dummyNode->next; // Next node becomes the new dummy

        pthread_mutex_unlock(&head_lock); // Unlock head mutex

        T curHeadVal = newDummyNode->value; // Value of current head
        head = newDummyNode; // Update head
        delete dummyNode; // Delete old dummy node
        pthread_mutex_unlock(&head_lock); // Unlock head mutex

        return curHeadVal; // Return the value
    }

    // Check if the queue is empty
    bool isEmpty() {        
        return head->next == nullptr;
    }

    // Print method to display queue values
    void print() {
        Node<T> *temp = head->next;  // Start from the first real node

        if (isEmpty()) {
            cout << "Empty";
        } else {
            while(temp){
                temp = temp->next;
            }
        }
    }

    private:
        Node<T> *head; // Pointer to the head node
        Node<T> *tail; // Pointer to the tail node
        pthread_mutex_t head_lock; // Mutex for head operations
        pthread_mutex_t tail_lock; // Mutex for tail operations
        
};

#endif /* QUEUE_H */
