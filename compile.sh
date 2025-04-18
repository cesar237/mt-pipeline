#!/bin/bash

echo "Compiling Shared Memory with debugging symbols and no optimization..."
gcc -g -O0 -lpthread mt-pipeline-shm.c -o mt-pipeline-shm

echo "Compiling Message Passing with debugging symbols and no optimization..."
gcc -g -O0 -lpthread mt-pipeline-msp.c -o mt-pipeline-msp

echo "Compiling Parallel with debugging symbols and no optimization..."
gcc -g -O0 -lpthread mt-parallel.c -o mt-parallel