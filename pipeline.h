#ifndef PIPELINE_H
#define PIPELINE_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "queue.h"

#define MAX_STAGES 128
#define MAX_THREADS 128

typedef void* (*stage_function_t)(void*);

// Structure to represent a pipeline stage
typedef struct {
    int stage_id;
    Queue* input_queue;
    Queue* output_queue;
    void* args;
    void* output;
    stage_function_t handler;
    int num_threads;
    pthread_t threads[MAX_THREADS];
    pthread_attr_t thread_attr;
    int priority;
    int cpu_affinity;
    bool is_running;
} Stage;

typedef struct {
    Stage* stage;
    void* args;
} ThreadArgs;

// Global pipeline configuration
typedef struct {
    int num_stages;
    Stage* stages[MAX_STAGES];
    bool pipeline_running;
} Pipeline;

Pipeline* create_pipeline();
void destroy_pipeline(Pipeline* pipeline);

Stage* create_stage(
    int stage_id, stage_function_t handler,
    void* args, void* output, int num_threads, int priority,
    Queue* input_queue, Queue* output_queue);
void destroy_stage(Stage* stage);

void start_stage(Stage* stage);
void stop_stage(Stage* stage);

bool add_pipeline_stage(Pipeline* pipeline, Stage* stage);

void start_pipeline(Pipeline* pipeline);
void stop_pipeline(Pipeline* pipeline);


Stage* create_stage(
    int stage_id,  stage_function_t handler,
    void* args, void* output, int num_threads, int priority,
    Queue* input_queue, Queue* output_queue) 
{
    ThreadArgs* thread_args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
    if (!thread_args) {
        perror("Failed to allocate memory for thread args");
        exit(EXIT_FAILURE);
    }
    Stage* stage = (Stage*)malloc(sizeof(Stage));
    if (!stage) {
        perror("Failed to allocate memory for stage");
        exit(EXIT_FAILURE);
    }
    thread_args->stage = stage;
    thread_args->args = args;

    stage->stage_id = stage_id;
    stage->args = thread_args;
    stage->output = output;
    stage->handler = handler;
    stage->num_threads = num_threads;
    stage->input_queue = input_queue;
    stage->output_queue = output_queue;
    stage->is_running = false;
    stage->priority = priority;
    stage->cpu_affinity = -1; // Default to no CPU affinity

    return stage;
}

void destroy_stage(Stage* stage) {
    if (!stage) return;
    free(stage);
}

bool add_pipeline_stage(Pipeline* pipeline, Stage* stage) {
    if (pipeline->num_stages >= MAX_STAGES) {
        fprintf(stderr, "Maximum number of stages reached\n");
        return false;
    }
    pipeline->stages[pipeline->num_stages++] = stage;

    printf("Stage %d added to pipeline\n", stage->stage_id);

    return true;
}

void start_stage(Stage* stage) {
    struct sched_param param;

    param.sched_priority = stage->priority;
    stage->is_running = true;

    ThreadArgs thread_args;
    thread_args.stage = stage;
    thread_args.args = stage->args;

    // Initialize thread attributes
    pthread_attr_init(&stage->thread_attr);
    pthread_attr_setschedparam(&stage->thread_attr, &param);

    for (int i = 0; i < stage->num_threads; i++) {
        if (pthread_create(
                &stage->threads[i],  &stage->thread_attr, 
                stage->handler, &thread_args)) 
        {
            fprintf(stderr, "Failed to create thread for stage %d\n",
                stage->stage_id);
            stage->is_running = false;
            return;
        }
    }

    printf("Stage %d started with %d threads\n", 
        stage->stage_id, stage->num_threads);
}

void stop_stage(Stage* stage) {
    stage->is_running = false;
    for (int i = 0; i < stage->num_threads; i++) {
        pthread_join(stage->threads[i], NULL);
    }
    pthread_attr_destroy(&stage->thread_attr);
    printf("Stage %d stopped\n", stage->stage_id);
}

void start_pipeline(Pipeline* pipeline) {
    pipeline->pipeline_running = true;

    printf("Starting pipeline with %d stages\n", pipeline->num_stages);
    for (int i = 0; i < pipeline->num_stages; i++) {
        printf("Starting stage %d\n", pipeline->stages[i]->stage_id);
        start_stage(pipeline->stages[i]);
    }

    printf("Pipeline started with %d stages\n", pipeline->num_stages);
}

void stop_pipeline(Pipeline* pipeline) {
    pipeline->pipeline_running = false;

    for (int i = 0; i < pipeline->num_stages; i++) {
        stop_stage(pipeline->stages[i]);
    }

    printf("Pipeline stopped\n");
}

Pipeline* create_pipeline() {
    Pipeline* pipeline = (Pipeline*)malloc(sizeof(Pipeline));
    if (!pipeline) {
        perror("Failed to allocate memory for pipeline");
        exit(EXIT_FAILURE);
    }
    pipeline->num_stages = 0;
    pipeline->pipeline_running = false;
    for (int i = 0; i < MAX_STAGES; i++) {
        pipeline->stages[i] = NULL;
    }
    return pipeline;
}

void destroy_pipeline(Pipeline* pipeline) {
    if (!pipeline) return;

    for (int i = 0; i < pipeline->num_stages; i++) {
        destroy_stage(pipeline->stages[i]);
    }
    free(pipeline);
    printf("Pipeline destroyed\n");
}

#endif // PIPELINE_H