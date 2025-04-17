#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>


uint64_t get_current_time();
void busy_poll_ns(uint64_t duration_ns);


// uint64_t get_current_time() {
//     struct timespec ts;
//     clock_gettime(CLOCK_REALTIME, &ts);
//     return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
// }

// Get current time in nanoseconds
uint64_t get_current_time(void) {
    struct timeval tv;
    
    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;
}


void busy_poll_ns(uint64_t duration_ns) {
    uint64_t start_time = get_current_time();
    while (get_current_time() - start_time < duration_ns) {
        // Busy wait
    }
}

#endif // DEBUG