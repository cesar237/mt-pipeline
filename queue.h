#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <stdatomic.h>

#include "pipeline.h"

// queue.h
// Structure to represent a queue
typedef struct {
    void** items;
    int capacity;
    int head;
    int tail;

    pthread_spinlock_t enqueue_lock;
    pthread_spinlock_t dequeue_lock;
} Queue;

// Function prototypes
Queue* create_queue(int capacity);
void destroy_queue(Queue* queue);
bool is_queue_empty(Queue* queue);
bool is_queue_full(Queue* queue);
int get_queue_size(Queue* queue);
int get_queue_capacity(Queue* queue);
bool enqueue(Queue* queue, void* item);
void* dequeue(Queue* queue);


Queue* create_queue(int capacity) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    if (!queue) {
        perror("Failed to allocate memory for queue");
        exit(EXIT_FAILURE);
    }

    queue->items = (void**)malloc(capacity * sizeof(void*));
    if (!queue->items) {
        perror("Failed to allocate memory for queue items");
        free(queue);
        exit(EXIT_FAILURE);
    }

    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = -1;

    if (pthread_spin_init(&queue->enqueue_lock, PTHREAD_PROCESS_PRIVATE) != 0) {
        perror("Failed to initialize enqueue lock");
        free(queue->items);
        free(queue);
        exit(EXIT_FAILURE);
    }
    if (pthread_spin_init(&queue->dequeue_lock, PTHREAD_PROCESS_PRIVATE) != 0) {
        perror("Failed to initialize dequeue lock");
        pthread_spin_destroy(&queue->enqueue_lock);
        free(queue->items);
        free(queue);
        exit(EXIT_FAILURE);
    }
    return queue;
}

void destroy_queue(Queue* queue) {
    if (queue) {
        // drain the queue
        int n = 0;
        while (!is_queue_empty(queue)) {
            void* item = dequeue(queue);
            if (item) {
                free(item);
                n++;
            }
        }
        printf("Drained %d items from the queue\n", n);
        pthread_spin_destroy(&queue->enqueue_lock);
        pthread_spin_destroy(&queue->dequeue_lock);
        free(queue->items);
        free(queue);
    }
}

int get_queue_capacity(Queue* queue) {
    return queue->capacity;
}

bool is_queue_empty(Queue* buffer) {
    return get_queue_size(buffer) == 0;
}

bool is_queue_full(Queue* rb) {
    return get_queue_size(rb) == rb->capacity - 1;
}

int get_queue_size(Queue* queue) {
    return (queue->head - queue->tail + queue->capacity) & (queue->capacity - 1);
}

bool enqueue(Queue* queue, void* item) {
    pthread_spin_lock(&queue->enqueue_lock);

    // If queue is full, return false
    if (is_queue_full(queue)) {
        pthread_spin_unlock(&queue->enqueue_lock);
        return false; 
    }

    // Add item to queue
    int index = queue->head & (queue->capacity - 1);
    queue->items[index] = item;
    if (queue->items[index]) {
        // Memory barrier to ensure the item is written before updating head
        __sync_synchronize();
        
        // Update head
        queue->head = (queue->head + 1) & (queue->capacity - 1);
    }
    // Memory barrier to ensure all writes are complete before unlocking
    __sync_synchronize();

    pthread_spin_unlock(&queue->enqueue_lock);
    return true;
}

void* dequeue(Queue* queue) {
    void* item = NULL;

    pthread_spin_lock(&queue->dequeue_lock);

    // If queue is empty, return NULL
    if (is_queue_empty(queue)) {
        pthread_spin_unlock(&queue->dequeue_lock);
        return NULL;
    } else {
        // Remove item from queue
        int index = queue->tail & (queue->capacity - 1);
        item = queue->items[index];
        queue->items[index] = NULL; // Clear the item
        // Memory barrier to ensure the item is read before updating tail
        __sync_synchronize();
        // Update tail
        queue->tail = (queue->tail + 1) & (queue->capacity - 1);
    }

    pthread_spin_unlock(&queue->dequeue_lock);
    return item;
}

#endif // !QUEUE_H
