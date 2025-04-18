#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <sched.h>
#include "pipeline.h"
#include "queue.h"
#include "simulation.h"

#define BATCH_SIZE 10
#define DEFAULT_PRIORITY 0

// Structure to represent an item in the pipeline
typedef struct {
    uint64_t id;
    // Additional fields can be added based on specific requirements
    uint64_t created_at;
} Item;

typedef struct{
    uint64_t tx;
    uint64_t drops;
} GenStats;

void* poll_jobs(void* arg) 
{
    printf("Poll jobs thread started\n");
    ThreadArgs* thread_args = (ThreadArgs*)arg;
    Queue* input_queue = thread_args->stage->input_queue;
    Queue* output_queue = thread_args->stage->output_queue;

    uint64_t duration = *(uint64_t*)thread_args->args;

    Item* item = NULL;

    while (thread_args->stage->is_running) {
        // Poll for items in the input queue
        item = (Item*)dequeue(input_queue);
        if (item) {
            // Process the item
            busy_poll_ns(duration);

            // Enqueue the processed item to the output queue
            enqueue(output_queue, item);
        }
        else{
            // If no item is available, sleep for a short duration
            usleep(10);
        }
    }
    return NULL;
}

typedef struct {
    Queue* queue;
    int duration_s;
    int batch_size;
} GeneratorArgs;

void* generate_items(void* arg) {
    GeneratorArgs* gen_args = (GeneratorArgs*)arg;
    Queue* queue = gen_args->queue;
    int duration_s = gen_args->duration_s;
    int batch_size = gen_args->batch_size;

    GenStats *stats;
    stats = (GenStats*)malloc(sizeof(GenStats));
    stats->tx = 0;
    stats->drops = 0;

    printf("Generator thread started. Should run for %d seconds.\n", duration_s);

    uint64_t start_time = get_current_time();
    uint64_t end_time = start_time + duration_s*1000000000;

    while (get_current_time() < end_time) {
        // printf("Current time: %lu\n", get_current_time());
        for (int i = 0; i < batch_size; i++) {
            // Create a new item
            Item* item = (Item*)malloc(sizeof(Item));
            item->id = stats->tx++;
            item->created_at = get_current_time();

            // Enqueue the item to the queue
            if(!enqueue(queue, item)){
                // If the queue is full, drop the item
                // printf("Item %lu dropped\n", item->id);
                stats->drops++;
                free(item);
            }
        }
        usleep(1); // Simulate item generation time
    }
    printf("Generator thread finished. Run in %lu nanoseconds.\n", 
        (get_current_time() - start_time));

    return (void *)stats;
}

void* sink(void* arg) {
    GeneratorArgs* gen_args = (GeneratorArgs*)arg;
    Queue* queue = gen_args->queue;
    int duration_s = gen_args->duration_s;

    printf("Sink thread started. Should run for %d seconds.\n", duration_s);

    uint64_t start_time = get_current_time();
    uint64_t end_time = start_time + duration_s*1000000000;
    Item* item = NULL;

    while (get_current_time() < end_time) {
        // Dequeue an item from the queue
        item = (Item*)dequeue(queue);
        if (item) {
            printf("%lu\n", get_current_time() - item->created_at);
            free(item);
        }
        else{
            // If no item is available, sleep for a short duration
            usleep(10);
        }
    }
    return NULL;
}

// Example usage
int main() {
    // Global pipeline instance
    Pipeline* pipeline;

    // generator and sink threads
    pthread_t generator_thread;
    pthread_t sink_thread;

    void* stats;
    // int num_stages = 3;
    
    // Example pipeline configuration
    pipeline = create_pipeline();
    
    
    // Create queues
    Queue *q1, *q2, *q3, *q4;
    q1 = create_queue(1024);
    q2 = create_queue(1024);
    q3 = create_queue(1024);
    q4 = create_queue(1024);

    //Create stages
    uint64_t duration_stage1 = 100;
    uint64_t duration_stage2 = 100;
    uint64_t duration_stage3 = 100;
    uint64_t num_threads = 1;
    Stage *stage1 = create_stage(
        1, (void *)&duration_stage1, poll_jobs, 
        NULL, num_threads, DEFAULT_PRIORITY, 
        q1, q2);
    Stage *stage2 = create_stage(
        2, (void *)&duration_stage2, poll_jobs,
        NULL, num_threads, DEFAULT_PRIORITY, 
        q2, q3);
    Stage *stage3 = create_stage(
        3, (void *)&duration_stage3, poll_jobs,
        NULL, num_threads, DEFAULT_PRIORITY, 
        q3, q4);

    // Add stages to the pipeline
    add_pipeline_stage(pipeline, stage1);
    // add_pipeline_stage(pipeline, stage2);
    // add_pipeline_stage(pipeline, stage3);

    GeneratorArgs gen_args = {q1, 10, BATCH_SIZE};
    GeneratorArgs sink_args = {q4, 10, BATCH_SIZE};

    if(pthread_create(&generator_thread, NULL, generate_items, 
        (void *)&gen_args)) {
        printf("Failed to create generator thread\n");
        return 1;
    }
    printf("Generator thread created\n");


    if(pthread_create(&sink_thread, NULL, sink, 
        (void *)&sink_args)) {
        printf("Failed to create sink thread\n");
        return 1;
    }
    printf("Sink thread created\n");

    // Start the pipeline
    start_pipeline(pipeline);
    // for (int i = 0; i < num_stages; i++) {
    //     start_stage(pipeline->stages[i], poll_jobs);
    // }

    // Stop and clean up
    pthread_join(generator_thread, &stats);
    pthread_join(sink_thread, NULL);
    stop_pipeline(pipeline);

    // Print statistics
    GenStats* gen_stats = (GenStats*)stats;
    printf("Generated items: %lu\n", gen_stats->tx);
    printf("Dropped items: %lu\n", gen_stats->drops);
    
    // Free allocated resources
    free(gen_stats);    
    destroy_queue(q1);
    destroy_queue(q2);
    destroy_queue(q3);
    destroy_queue(q4);
    destroy_pipeline(pipeline);

    return 0;
}