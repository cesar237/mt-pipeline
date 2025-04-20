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

#include "argparse.h"
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
    uint64_t finished_at;
    bool steps_ok[MAX_STAGES];
    uint64_t stage_time[MAX_STAGES];
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
            item->stage_time[stage->stage_id] = get_current_time();
            busy_poll_ns(duration);
            item->finished_at = get_current_time();

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
    uint64_t end_time = start_time + duration_s*1000000000ULL;

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
    uint64_t end_time = start_time + duration_s*1000000000ULL;
    uint64_t total_latency = 0;
    uint64_t total_wait_time = 0;
    uint64_t total_processing_time = 0;
    uint64_t total_sink_time = 0;
    Item* item = NULL;

    while (get_current_time() < end_time) {
        // Dequeue an item from the queue
        item = (Item*)dequeue(queue);
        if (item) {
            processed++;
            uint64_t got = get_current_time();
            // printf("%lu\n", get_current_time() - item->created_at);
            total_latency += (got - item->created_at) / 1000;
            total_wait_time += (item->stage_time[1] - item->created_at) / 1000;
            total_sink_time += (got - item->finished_at) / 1000;
            total_processing_time += (item->finished_at - item->stage_time[1]) / 1000;
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
    printf("Average wait time: %.2f microseconds\n", total_wait_time / (double)processed);
    printf("Average sink time: %.2f microseconds\n", total_wait_time / (double)processed);
    printf("Average processing time: %.2f microseconds\n", total_processing_time / (double)processed);

    return NULL;
}


// Example usage
int main(int argc, const char **argv) {
    #define N_THREADS 10
    #define DURATION_STAGE 10000
    #define PRIORITY 0
    #define QUEUE_SIZE 1024

    int n_threads = 10;
    int stage_duration = 1000;
    int base_priority = 0;
    int queue_size = 1024;
    int expe_duration = 10;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_INTEGER('n', "threads", &n_threads, "Number of stage threads", NULL),
        OPT_INTEGER('s', "service-time", &stage_duration, "Service time per stage", NULL),
        OPT_INTEGER('q', "queue-size", &queue_size, "Queue Size", NULL),
        OPT_INTEGER('d', "duration", &expe_duration, "Duration of the experiment", NULL),
        OPT_END(),
    };

    struct argparse parser;
    argparse_init(&parser, options, NULL, 0);
    argc = argparse_parse(&parser, argc, argv);

    // Global pipeline instance
    Pipeline* pipeline;

    // generator and sink threads
    pthread_t generator_thread;
    pthread_t sink_thread;
    pthread_t stage1_threads[MAX_THREADS];

    int num_threads1 = n_threads;

    //Create stages
    uint64_t duration_stage1 = stage_duration;
    uint64_t duration_stage2 = stage_duration;
    uint64_t duration_stage3 = stage_duration;

    uint64_t duration_stage = duration_stage1 + duration_stage2 + duration_stage3;

    void* stats;
    
    // Example pipeline configuration
    pipeline = create_pipeline();
    
    // Create queues
    Queue *q1, *q2;
    q1 = create_queue(queue_size);
    q2 = create_queue(queue_size);

    Stage *stage1 = create_stage(
        1, (void *)&duration_stage, poll_jobs, 
        NULL, num_threads1, DEFAULT_PRIORITY, 
        q1, q2);

    GeneratorArgs gen_args = {q1, expe_duration, BATCH_SIZE};
    GeneratorArgs sink_args = {q2, expe_duration, BATCH_SIZE};

    int priority1 = PRIORITY;
    ThreadArgs thread1_args = {stage1, &duration_stage, &priority1};

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
        // pin_thread_to_cpu(stage1_threads[i], i + 2);
    }
    printf("Stage1 threads created\n");

    // Stop and clean up
    pthread_join(generator_thread, &stats);
    pthread_join(sink_thread, NULL);
    stage1->is_running = false;
    for (int i = 0; i < num_threads1; i++) {
        pthread_join(stage1_threads[i], NULL);
    }

    // Print statistics
    printf("Total duration stage should be %lu microseconds\n", 
        (duration_stage) / 1000);

    GenStats* gen_stats = (GenStats*)stats;
    printf("Generated items: %lu\n", gen_stats->tx);
    printf("Dropped items: %lu\n", gen_stats->drops);
    
    // Free allocated resources
    free(gen_stats);    
    destroy_queue(q1);
    destroy_queue(q2);
    destroy_pipeline(pipeline);

    return 0;
}