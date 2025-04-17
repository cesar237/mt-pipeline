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
#include "pipeline.h"

// queue.h
// Structure to represent a queue
typedef struct {
    void** items;
    int capacity;
    int size;
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
    queue->size = 0;
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
        pthread_spin_destroy(&queue->enqueue_lock);
        pthread_spin_destroy(&queue->dequeue_lock);
        free(queue->items);
        free(queue);
    }
}

int get_queue_size(Queue* buffer) {
    return (buffer->tail - buffer->head + buffer->capacity) % buffer->capacity;
}

bool is_queue_empty(Queue* buffer) {
    return get_queue_size(buffer) == 0;
}

bool is_queue_full(Queue* buffer) {
    return get_queue_size(buffer) == buffer->capacity;
}

bool enqueue(Queue* queue, void* item) {
    bool result = true;

    pthread_spin_lock(&queue->enqueue_lock);

    // If queue is full, ecrase the oldest item
    if (is_queue_full(queue)) {
        result = false; 
    }

    // Add item to queue
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->items[queue->tail] = item;
    queue->size++;

    pthread_spin_unlock(&queue->enqueue_lock);
    return result;
}

void* dequeue(Queue* queue) {
    void* item = NULL;

    pthread_spin_lock(&queue->dequeue_lock);

    // If queue is empty, return NULL
    if (is_queue_empty(queue)) {
        item = NULL;
    } else {
        // Remove item from queue
        item = queue->items[queue->head];
        queue->head = (queue->head + 1) % queue->capacity;
        queue->size--;
    }

    pthread_spin_unlock(&queue->dequeue_lock);
    return item;
}

#endif // !QUEUE_H
