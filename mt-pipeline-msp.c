#define _GNU_SOURCE


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

#define BATCH_SIZE 64
#define DEFAULT_PRIORITY 0
#define WAIT_PREVIOUS_STAGE_US 1

// Structure to represent an item in the pipeline
typedef struct {
    uint64_t id;
    // Additional fields can be added based on specific requirements
    uint64_t created_at;
    bool steps_ok[MAX_STAGES];
} Item;

typedef struct{
    uint64_t tx;
    uint64_t drops;
} GenStats;

void* poll_jobs(void* arg)  {
    // printf("Poll jobs thread started\n");
    ThreadArgs* thread_args = (ThreadArgs*)arg;
    Queue* input_queue = thread_args->stage->input_queue;
    Queue* output_queue = thread_args->stage->output_queue;
    int priority = *(int*)thread_args->priority;
    Stage* stage = thread_args->stage;

    // Set thread priority
    set_current_thread_nice_level(priority);

    uint64_t forwarded = 0;
    uint64_t dropped = 0;

    uint64_t duration = *(uint64_t*)thread_args->args;

    Item* item = NULL;

    while (stage->is_running) {
        // Poll for items in the input queue
        item = (Item*)dequeue(input_queue);
        if (item) {
            busy_poll_ns(duration);

            // Enqueue the processed item to the output queue
            if(enqueue(output_queue, item)) {
                forwarded++;
            }else{
                // If the output queue is full, drop the item
                dropped++;
                free(item);
            }
        }
        else{
            // If no item is available, sleep for a short duration
            usleep(WAIT_PREVIOUS_STAGE_US);
        }
    }
    // printf("Stage %d thread finished. Forwarded: %lu, Dropped: %lu\n", 
    //     thread_args->stage->stage_id, forwarded, dropped);
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

    // set_current_thread_nice_level(-10);

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
            for (int j = 0; j < 10; j++) {
                item->steps_ok[j] = false;
            }

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
    uint64_t processed = 0;

    // set_current_thread_nice_level(-10);

    printf("Sink thread started. Should run for %d seconds.\n", duration_s);

    uint64_t start_time = get_current_time();
    uint64_t end_time = start_time + duration_s*1000000000;
    uint64_t total_latency = 0;
    Item* item = NULL;

    while (get_current_time() < end_time) {
        // Dequeue an item from the queue
        item = (Item*)dequeue(queue);
        if (item) {
            processed++;
            // printf("%lu\n", get_current_time() - item->created_at);
            total_latency += (get_current_time() - item->created_at) / 1000;
            free(item);
        }
        else{
            // If no item is available, sleep for a short duration
            usleep(10);
        }
    }
    printf("Sink thread finished. Processed: %lu, total latency: %lu microseconds\n", 
        processed, total_latency);
    printf("Average latency: %.2f microseconds\n", total_latency / (double)processed);
    return NULL;
}

// Example usage
int main() {
    // Global pipeline instance
    Pipeline* pipeline;

    // generator and sink threads
    pthread_t generator_thread;
    pthread_t sink_thread;
    pthread_t stage1_threads[MAX_THREADS];
    pthread_t stage2_threads[MAX_THREADS];
    pthread_t stage3_threads[MAX_THREADS];

    #define N_THREADS 5
    #define DURATION_STAGE 100000
    #define PRIORITY 0
    #define QUEUE_SIZE 1024

    int num_threads1 = N_THREADS;
    int num_threads2 = N_THREADS;
    int num_threads3 = N_THREADS;

    //Create stages
    uint64_t duration_stage1 = DURATION_STAGE;
    uint64_t duration_stage2 = DURATION_STAGE;
    uint64_t duration_stage3 = DURATION_STAGE;

    // Priority levels
    int priority1 = PRIORITY;
    int priority2 = PRIORITY;
    int priority3 = PRIORITY;

    void* stats;
    // int num_stages = 3;
    
    // Example pipeline configuration
    pipeline = create_pipeline();
    
    
    // Create queues
    Queue *q1, *q2, *q3, *q4;
    q1 = create_queue(QUEUE_SIZE);
    q2 = create_queue(QUEUE_SIZE);
    q3 = create_queue(QUEUE_SIZE);
    q4 = create_queue(QUEUE_SIZE);

    Stage *stage1 = create_stage(
        1, (void *)&duration_stage1, poll_jobs, 
        NULL, num_threads1, DEFAULT_PRIORITY, 
        q1, q2);
    Stage *stage2 = create_stage(
        2, (void *)&duration_stage2, poll_jobs,
        NULL, num_threads2, DEFAULT_PRIORITY, 
        q2, q3);
    Stage *stage3 = create_stage(
        3, (void *)&duration_stage3, poll_jobs,
        NULL, num_threads3, DEFAULT_PRIORITY, 
        q3, q4);

    // Add stages to the pipeline
    // add_pipeline_stage(pipeline, stage1);
    // add_pipeline_stage(pipeline, stage2);
    // add_pipeline_stage(pipeline, stage3);

    GeneratorArgs gen_args = {q1, 15, BATCH_SIZE};
    GeneratorArgs sink_args = {q4, 15, BATCH_SIZE};

    ThreadArgs thread1_args = {stage1, &duration_stage1, &priority1};
    ThreadArgs thread2_args = {stage2, &duration_stage2, &priority2};
    ThreadArgs thread3_args = {stage3, &duration_stage3, &priority3};

    if(pthread_create(&generator_thread, NULL, generate_items, 
        (void *)&gen_args)) {
        printf("Failed to create generator thread\n");
        return 1;
    }
    pin_thread_to_cpu(generator_thread, 0);
    printf("Generator thread created\n");

    
    if(pthread_create(&sink_thread, NULL, sink, 
        (void *)&sink_args)) {
        printf("Failed to create sink thread\n");
        return 1;
    }
    pin_thread_to_cpu(sink_thread, 1);
    printf("Sink thread created\n");

    // Create stage1 threads
    stage1->is_running = true;
    for (int i = 0; i < num_threads1; i++) {
        if (pthread_create(&stage1_threads[i], NULL, poll_jobs, 
            (void *)&thread1_args)) {
            printf("Failed to create stage1 thread\n");
            return 1;
        }
        pin_thread_to_cpu(stage1_threads[i], i + 2);
    }
    printf("Stage1 threads created\n");
    
    // Create stage2 threads
    stage2->is_running = true;
    for (int i = 0; i < num_threads2; i++) {
        if (pthread_create(&stage2_threads[i], NULL, poll_jobs, 
            (void *)&thread2_args)) {
            printf("Failed to create stage2 thread\n");
            return 1;
        }
        pin_thread_to_cpu(stage2_threads[i], i + 2 + num_threads1);
    }
    printf("Stage2 threads created\n");

    // Create stage3 threads
    stage3->is_running = true;
    for (int i = 0; i < num_threads3; i++) {
        if (pthread_create(&stage3_threads[i], NULL, poll_jobs, 
            (void *)&thread3_args)) {
            printf("Failed to create stage3 thread\n");
            return 1;
        }
        pin_thread_to_cpu(stage3_threads[i], i + 2 + num_threads1 + num_threads2);
    }
    printf("Stage3 threads created\n");

    // Start the pipeline
    // start_pipeline(pipeline);
    // for (int i = 0; i < num_stages; i++) {
    //     start_stage(pipeline->stages[i], poll_jobs);
    // }

    // Stop and clean up
    pthread_join(generator_thread, &stats);
    pthread_join(sink_thread, NULL);
    stage1->is_running = false;
    stage2->is_running = false;
    stage3->is_running = false;
    for (int i = 0; i < num_threads1; i++) {
        pthread_join(stage1_threads[i], NULL);
    }
    for (int i = 0; i < num_threads2; i++) {
        pthread_join(stage2_threads[i], NULL);
    }
    for (int i = 0; i < num_threads3; i++) {
        pthread_join(stage3_threads[i], NULL);
    }
    // stop_pipeline(pipeline);

    // Print statistics
    printf("Total duration stage should be %lu microseconds\n", 
        (duration_stage1 + duration_stage2 + duration_stage3) / 1000);

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